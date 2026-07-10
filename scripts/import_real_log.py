import argparse
import bisect
import hashlib
import os
import re
import sqlite3
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime


HEADER_RE = re.compile(
    r"^(?P<time>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\s+"
    r"(?P<level>DEBUG|INFO|WARN|ERROR)\s+"
    r"\[(?P<thread_id>\d+)\]\s+"
    r"\[(?P<source_symbol>[^\]]+)\]\s+"
    r"(?P<message>.*)$"
)

DETECT_DONE_RE = re.compile(r"相机 (?P<camera>\S+) 触发ID (?P<trigger>\d+) 检测完成，isOK = (?P<is_ok>\d+), 结果 = (?P<result>.+)$")
RECEIVE_RE = re.compile(r"相机 (?P<camera>\S+), 物料 (?P<material>\d+), 触发->收图 (?P<ms>\d+)ms")
GOTO_DETECT_RE = re.compile(r"相机 (?P<camera>\S+), 物料 (?P<material>\d+), 收图->toDetect (?P<ms>\d+)ms")
GET_RESULT_RE = re.compile(r"相机 (?P<camera>\S+), 物料 (?P<material>\d+), toDetect->getResult (?P<ms>\d+)ms")
PRECHECK_RE = re.compile(r"预检查完毕，预计可输入深度学习的图像数量 = (?P<count>\d+)")
INFER_START_RE = re.compile(r"深度学习接口开始推理，inImageSize = (?P<count>\d+)")
INFER_END_RE = re.compile(r"深度学习接口推理结束, outDefectListSize = (?P<count>\d+), 耗时ms = (?P<ms>\d+)")
BRIDGE_CHANGED_RE = re.compile(r"BridgeDataChanged: (?P<value>\d+)")
TRIGGER_ALL_RE = re.compile(r"PLC信号桥收到 (?P<value>\d+), 触发所有相机")
PLC_WRITE_RE = re.compile(r"Write \(DB: (?P<db>\d+), Offset: (?P<offset>\d+)\) to value\((?P<value>\d+)\)")
PUSH_ITEM_RE = re.compile(r"正在推出一个数据结果，cameraUUID = (?P<camera>\S+)")

TIME_FORMAT = "%Y-%m-%d %H:%M:%S.%f"


@dataclass
class StageData:
    material_id: str = ""
    receive_time: str = ""
    receive_ms: int | None = None
    receive_line_id: int | None = None
    goto_time: str = ""
    goto_ms: int | None = None
    goto_line_id: int | None = None
    result_time: str = ""
    result_ms: int | None = None
    result_line_id: int | None = None


@dataclass
class ItemData:
    trigger_id: str
    camera_uuid: str
    detect_finish_time: str
    is_ok: int
    result_raw: str
    source_detect_line_id: int
    material_id: str = ""
    receive_image_time: str = ""
    go_to_detect_time: str = ""
    result_received_time: str = ""
    trigger_to_receive_ms: int | None = None
    receive_to_detect_ms: int | None = None
    to_detect_to_result_ms: int | None = None
    result_primary_code: str = ""
    defect_count_derived: int = 0
    item_id: int | None = None


@dataclass
class TriggerData:
    trigger_id: str
    material_id: str = ""
    trigger_seq: int = 0
    plc_signal_value: int | None = None
    bridge_event_time: str = ""
    trigger_all_camera_time: str = ""
    first_receive_time: str = ""
    first_detect_finish_time: str = ""
    expected_camera_count: int = 4
    actual_camera_count: int = 0
    status: str = ""
    link_method: str = "trigger_id_from_detection_result"
    link_confidence: float = 1.0
    trigger_pk: int | None = None


@dataclass
class InferenceRunData:
    precheck_time: str = ""
    input_image_count: int | None = None
    start_time: str = ""
    end_time: str = ""
    output_result_count: int | None = None
    infer_ms: int | None = None
    thread_id: int | None = None
    source_precheck_line_id: int | None = None
    source_start_line_id: int | None = None
    source_end_line_id: int | None = None
    trigger_id: str = ""
    trigger_pk: int | None = None
    run_id: int | None = None
    link_method: str = "sequential_inference_before_detection"
    link_confidence: float = 0.9


@dataclass
class EquipmentEventData:
    event_time: str
    level: str
    thread_id: int | None
    source_symbol: str
    event_category: str
    event_type: str
    message: str
    source_line_id: int
    signal_value: int | None = None
    db_no: int | None = None
    offset_no: int | None = None
    value: int | None = None
    linked_trigger_id: str = ""
    linked_trigger_pk: int | None = None
    link_method: str = ""
    link_confidence: float | None = None
    event_id: int | None = None


@dataclass
class ImportState:
    first_event_time: str = ""
    last_event_time: str = ""
    line_count: int = 0
    cameras: set[str] = field(default_factory=set)
    triggers: dict[str, TriggerData] = field(default_factory=dict)
    stage_by_key: dict[tuple[str, str], StageData] = field(default_factory=dict)
    items: dict[tuple[str, str], ItemData] = field(default_factory=dict)
    inference_runs: list[InferenceRunData] = field(default_factory=list)
    pending_inference: InferenceRunData | None = None
    equipment_events: list[EquipmentEventData] = field(default_factory=list)
    detect_line_id_by_item: dict[tuple[str, str], int] = field(default_factory=dict)


def parse_time(value: str) -> datetime:
    return datetime.strptime(value, TIME_FORMAT)


def split_source_symbol(source_symbol: str) -> tuple[str, str, int | None]:
    source_line_no = None
    symbol = source_symbol
    match = re.match(r"^(?P<symbol>.*)@(?P<line>\d+)$", source_symbol)
    if match:
        symbol = match.group("symbol")
        source_line_no = int(match.group("line"))

    parts = symbol.split("::")
    class_name = parts[0] if parts else symbol
    function_name = "::".join(parts[1:]) if len(parts) > 1 else symbol
    return class_name, function_name, source_line_no


def sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def first_result_code(result_raw: str) -> str:
    tokens = [token.strip() for token in result_raw.split(",") if token.strip()]
    return tokens[0] if tokens else result_raw.strip()


def defect_tokens(result_raw: str) -> list[str]:
    return [token.strip() for token in result_raw.split(",") if token.strip() and token.strip() != "OK"]


def ensure_trigger(state: ImportState, trigger_id: str) -> TriggerData:
    if trigger_id not in state.triggers:
        state.triggers[trigger_id] = TriggerData(trigger_id=trigger_id)
    return state.triggers[trigger_id]


def classify_equipment_event(event_time, level, thread_id, source_symbol, message, line_id) -> EquipmentEventData | None:
    if match := BRIDGE_CHANGED_RE.search(message):
        return EquipmentEventData(
            event_time=event_time,
            level=level,
            thread_id=thread_id,
            source_symbol=source_symbol,
            event_category="PLC",
            event_type="plc_bridge_changed",
            message=message,
            source_line_id=line_id,
            signal_value=int(match.group("value")),
        )
    if match := TRIGGER_ALL_RE.search(message):
        return EquipmentEventData(
            event_time=event_time,
            level=level,
            thread_id=thread_id,
            source_symbol=source_symbol,
            event_category="PLC",
            event_type="plc_trigger_all_cameras",
            message=message,
            source_line_id=line_id,
            signal_value=int(match.group("value")),
        )
    if match := PLC_WRITE_RE.search(message):
        offset_no = int(match.group("offset"))
        return EquipmentEventData(
            event_time=event_time,
            level=level,
            thread_id=thread_id,
            source_symbol=source_symbol,
            event_category="PLC",
            event_type="plc_write",
            message=message,
            source_line_id=line_id,
            db_no=int(match.group("db")),
            offset_no=offset_no,
            value=int(match.group("value")),
        )
    if "批次缓存结果重置" in message:
        return EquipmentEventData(event_time, level, thread_id, source_symbol, "PROCESS", "batch_cache_reset", message, line_id)
    if "业务主干推出一个批次的结果 开始" in message:
        return EquipmentEventData(event_time, level, thread_id, source_symbol, "PROCESS", "result_push_start", message, line_id)
    if "业务主干向PLC发送批次处理结果" in message:
        return EquipmentEventData(event_time, level, thread_id, source_symbol, "PROCESS", "result_push_send_plc", message, line_id)
    if PUSH_ITEM_RE.search(message):
        return EquipmentEventData(event_time, level, thread_id, source_symbol, "PROCESS", "result_push_item", message, line_id)
    if "removeOldImageDir" in source_symbol or "删除" in message or "清理" in message:
        return EquipmentEventData(event_time, level, thread_id, source_symbol, "SYSTEM", "image_cleanup", message, line_id)
    if level in ("WARN", "ERROR"):
        return EquipmentEventData(event_time, level, thread_id, source_symbol, "SYSTEM", "runtime_warning", message, line_id)
    return None


def link_inference_run(state: ImportState, trigger_id: str) -> None:
    for run in reversed(state.inference_runs):
        if not run.trigger_id:
            run.trigger_id = trigger_id
            return
        if run.trigger_id == trigger_id:
            return


def process_business_event(state: ImportState, event_time: str, level: str, thread_id: int | None, source_symbol: str, message: str, line_id: int) -> str:
    event_type = "generic"

    if event := classify_equipment_event(event_time, level, thread_id, source_symbol, message, line_id):
        state.equipment_events.append(event)
        event_type = event.event_type

    if match := PRECHECK_RE.search(message):
        state.pending_inference = InferenceRunData(
            precheck_time=event_time,
            input_image_count=int(match.group("count")),
            thread_id=thread_id,
            source_precheck_line_id=line_id,
        )
        return "inference_precheck"

    if match := INFER_START_RE.search(message):
        if not state.pending_inference:
            state.pending_inference = InferenceRunData()
        state.pending_inference.start_time = event_time
        state.pending_inference.input_image_count = int(match.group("count"))
        state.pending_inference.thread_id = thread_id
        state.pending_inference.source_start_line_id = line_id
        return "inference_start"

    if match := INFER_END_RE.search(message):
        run = state.pending_inference or InferenceRunData()
        run.end_time = event_time
        run.output_result_count = int(match.group("count"))
        run.infer_ms = int(match.group("ms"))
        run.thread_id = thread_id
        run.source_end_line_id = line_id
        state.inference_runs.append(run)
        state.pending_inference = None
        return "inference_end"

    if match := RECEIVE_RE.search(message):
        camera = match.group("camera")
        material = match.group("material")
        state.cameras.add(camera)
        stage = state.stage_by_key.setdefault((material, camera), StageData(material_id=material))
        stage.receive_time = event_time
        stage.receive_ms = int(match.group("ms"))
        stage.receive_line_id = line_id
        return "receive_image"

    if match := GOTO_DETECT_RE.search(message):
        camera = match.group("camera")
        material = match.group("material")
        state.cameras.add(camera)
        stage = state.stage_by_key.setdefault((material, camera), StageData(material_id=material))
        stage.goto_time = event_time
        stage.goto_ms = int(match.group("ms"))
        stage.goto_line_id = line_id
        return "go_to_detect"

    if match := GET_RESULT_RE.search(message):
        camera = match.group("camera")
        material = match.group("material")
        state.cameras.add(camera)
        stage = state.stage_by_key.setdefault((material, camera), StageData(material_id=material))
        stage.result_time = event_time
        stage.result_ms = int(match.group("ms"))
        stage.result_line_id = line_id
        return "result_received"

    if match := DETECT_DONE_RE.search(message):
        camera = match.group("camera")
        trigger_id = match.group("trigger")
        result_raw = match.group("result").strip()
        is_ok = int(match.group("is_ok"))
        state.cameras.add(camera)
        trigger = ensure_trigger(state, trigger_id)
        stage = state.stage_by_key.get((trigger_id, camera), StageData(material_id=trigger_id))
        trigger.material_id = trigger.material_id or stage.material_id or trigger_id
        item = ItemData(
            trigger_id=trigger_id,
            camera_uuid=camera,
            detect_finish_time=event_time,
            is_ok=is_ok,
            result_raw=result_raw,
            source_detect_line_id=line_id,
            material_id=stage.material_id or trigger_id,
            receive_image_time=stage.receive_time,
            go_to_detect_time=stage.goto_time,
            result_received_time=stage.result_time,
            trigger_to_receive_ms=stage.receive_ms,
            receive_to_detect_ms=stage.goto_ms,
            to_detect_to_result_ms=stage.result_ms,
            result_primary_code=first_result_code(result_raw),
            defect_count_derived=len(defect_tokens(result_raw)),
        )
        state.items[(trigger_id, camera)] = item
        state.detect_line_id_by_item[(trigger_id, camera)] = line_id
        link_inference_run(state, trigger_id)
        return "detect_done"

    return event_type


def nearest_trigger_by_time(triggers: list[TriggerData], event_time: str, max_seconds: float) -> TriggerData | None:
    candidates = []
    for trigger in triggers:
        anchor = trigger.trigger_all_camera_time or trigger.first_receive_time or trigger.first_detect_finish_time
        if not anchor:
            continue
        delta = abs((parse_time(event_time) - parse_time(anchor)).total_seconds())
        if delta <= max_seconds:
            candidates.append((delta, trigger))
    if not candidates:
        return None
    candidates.sort(key=lambda item: item[0])
    return candidates[0][1]


def finalize_links(state: ImportState) -> None:
    trigger_all_events = [event for event in state.equipment_events if event.event_type == "plc_trigger_all_cameras"]
    bridge_events = [event for event in state.equipment_events if event.event_type == "plc_bridge_changed"]

    for trigger in state.triggers.values():
        related_items = [item for item in state.items.values() if item.trigger_id == trigger.trigger_id]
        related_items.sort(key=lambda item: item.detect_finish_time)
        trigger.actual_camera_count = len(related_items)
        trigger.status = "complete" if trigger.actual_camera_count == trigger.expected_camera_count else "partial"
        receive_times = [item.receive_image_time for item in related_items if item.receive_image_time]
        detect_times = [item.detect_finish_time for item in related_items if item.detect_finish_time]
        trigger.first_receive_time = min(receive_times) if receive_times else ""
        trigger.first_detect_finish_time = min(detect_times) if detect_times else ""

        if trigger.first_receive_time:
            trigger_anchor = parse_time(trigger.first_receive_time)
            prior_trigger_events = [
                event for event in trigger_all_events
                if 0 <= (trigger_anchor - parse_time(event.event_time)).total_seconds() <= 3
            ]
            if prior_trigger_events:
                event = prior_trigger_events[-1]
                trigger.trigger_all_camera_time = event.event_time
                trigger.plc_signal_value = event.signal_value
                event.linked_trigger_id = trigger.trigger_id
                event.link_method = "nearest_trigger_before_receive"
                event.link_confidence = 0.9

            prior_bridge_events = [
                event for event in bridge_events
                if 0 <= (trigger_anchor - parse_time(event.event_time)).total_seconds() <= 3
            ]
            if prior_bridge_events:
                event = prior_bridge_events[-1]
                trigger.bridge_event_time = event.event_time
                if not trigger.plc_signal_value:
                    trigger.plc_signal_value = event.signal_value
                event.linked_trigger_id = trigger.trigger_id
                event.link_method = "nearest_bridge_before_receive"
                event.link_confidence = 0.75

        if not trigger.trigger_all_camera_time:
            trigger.trigger_all_camera_time = trigger.first_receive_time or trigger.first_detect_finish_time
            trigger.link_method = "fallback_from_item_time"
            trigger.link_confidence = 0.65

    sorted_triggers = sorted(
        state.triggers.values(),
        key=lambda trigger: trigger.trigger_all_camera_time or trigger.first_detect_finish_time or trigger.trigger_id,
    )
    for index, trigger in enumerate(sorted_triggers, start=1):
        trigger.trigger_seq = index

    for run in state.inference_runs:
        if run.trigger_id and run.trigger_id in state.triggers:
            continue
        linked = nearest_trigger_by_time(sorted_triggers, run.end_time or run.start_time, 2.0) if (run.end_time or run.start_time) else None
        if linked:
            run.trigger_id = linked.trigger_id
            run.link_method = "nearest_trigger_to_inference_time"
            run.link_confidence = 0.75

    for event in state.equipment_events:
        if event.linked_trigger_id:
            continue
        linked = nearest_trigger_by_time(sorted_triggers, event.event_time, 5.0)
        if linked:
            event.linked_trigger_id = linked.trigger_id
            event.link_method = "nearest_trigger_time_window"
            event.link_confidence = 0.6


def insert_evidence(conn: sqlite3.Connection, entity_type: str, entity_id: int | None, role: str, line_id: int | None) -> None:
    if entity_id is None or line_id is None:
        return
    conn.execute(
        "INSERT OR IGNORE INTO entity_evidence(entity_type, entity_id, evidence_role, line_id) VALUES (?, ?, ?, ?)",
        (entity_type, entity_id, role, line_id),
    )


def import_log(args: argparse.Namespace) -> None:
    log_path = os.path.abspath(args.log)
    db_path = os.path.abspath(args.db)
    schema_path = os.path.abspath(args.schema)

    if os.path.exists(db_path):
        os.remove(db_path)

    state = ImportState()
    file_hash = sha256_file(log_path)
    file_size = os.path.getsize(log_path)

    conn = sqlite3.connect(db_path)
    conn.execute("PRAGMA foreign_keys = ON")
    with open(schema_path, "r", encoding="utf-8-sig") as schema_file:
        conn.executescript(schema_file.read())

    import_row = conn.execute(
        """
        INSERT INTO log_import(project_id, file_path, file_name, file_size, sha256)
        VALUES (?, ?, ?, ?, ?)
        """,
        (args.project_id, log_path, os.path.basename(log_path), file_size, file_hash),
    )
    import_id = import_row.lastrowid

    with conn:
        with open(log_path, "r", encoding="utf-8-sig", errors="replace") as log_file:
            for line_no, raw_line in enumerate(log_file, start=1):
                raw_text = raw_line.rstrip("\r\n")
                state.line_count += 1
                event_time = None
                level = None
                thread_id = None
                source_symbol = None
                class_name = None
                function_name = None
                source_line_no = None
                message = raw_text
                parse_status = "unmatched"
                event_type = "unmatched"

                match = HEADER_RE.match(raw_text)
                if match:
                    event_time = match.group("time")
                    level = match.group("level")
                    thread_id = int(match.group("thread_id"))
                    source_symbol = match.group("source_symbol")
                    message = match.group("message")
                    class_name, function_name, source_line_no = split_source_symbol(source_symbol)
                    parse_status = "parsed_header"
                    if not state.first_event_time:
                        state.first_event_time = event_time
                    state.last_event_time = event_time

                cursor = conn.execute(
                    """
                    INSERT INTO raw_log_line(
                        import_id, line_no, event_time, level, thread_id, source_symbol,
                        class_name, function_name, source_line_no, message, event_type, parse_status, raw_text
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        import_id,
                        line_no,
                        event_time,
                        level,
                        thread_id,
                        source_symbol,
                        class_name,
                        function_name,
                        source_line_no,
                        message,
                        event_type,
                        parse_status,
                        raw_text,
                    ),
                )
                line_id = cursor.lastrowid

                if match:
                    event_type = process_business_event(state, event_time, level, thread_id, source_symbol, message, line_id)
                    conn.execute("UPDATE raw_log_line SET event_type = ? WHERE line_id = ?", (event_type, line_id))

        finalize_links(state)

        conn.execute(
            "UPDATE log_import SET first_event_time = ?, last_event_time = ? WHERE import_id = ?",
            (state.first_event_time, state.last_event_time, import_id),
        )

        for camera_uuid in sorted(state.cameras):
            conn.execute(
                "INSERT OR IGNORE INTO camera(camera_uuid, camera_name) VALUES (?, ?)",
                (camera_uuid, camera_uuid),
            )

        for trigger in sorted(state.triggers.values(), key=lambda item: item.trigger_seq):
            cursor = conn.execute(
                """
                INSERT INTO inspection_trigger(
                    import_id, trigger_id, material_id, trigger_seq, plc_signal_value,
                    bridge_event_time, trigger_all_camera_time, first_receive_time,
                    first_detect_finish_time, expected_camera_count, actual_camera_count,
                    status, link_method, link_confidence
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    import_id,
                    trigger.trigger_id,
                    trigger.material_id,
                    trigger.trigger_seq,
                    trigger.plc_signal_value,
                    trigger.bridge_event_time,
                    trigger.trigger_all_camera_time,
                    trigger.first_receive_time,
                    trigger.first_detect_finish_time,
                    trigger.expected_camera_count,
                    trigger.actual_camera_count,
                    trigger.status,
                    trigger.link_method,
                    trigger.link_confidence,
                ),
            )
            trigger.trigger_pk = cursor.lastrowid

        for run in state.inference_runs:
            if run.trigger_id and run.trigger_id in state.triggers:
                run.trigger_pk = state.triggers[run.trigger_id].trigger_pk
            cursor = conn.execute(
                """
                INSERT INTO inference_run(
                    import_id, trigger_pk, precheck_time, input_image_count, start_time,
                    end_time, output_result_count, infer_ms, thread_id, link_method, link_confidence
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    import_id,
                    run.trigger_pk,
                    run.precheck_time,
                    run.input_image_count,
                    run.start_time,
                    run.end_time,
                    run.output_result_count,
                    run.infer_ms,
                    run.thread_id,
                    run.link_method,
                    run.link_confidence,
                ),
            )
            run.run_id = cursor.lastrowid
            insert_evidence(conn, "inference_run", run.run_id, "precheck", run.source_precheck_line_id)
            insert_evidence(conn, "inference_run", run.run_id, "start", run.source_start_line_id)
            insert_evidence(conn, "inference_run", run.run_id, "end", run.source_end_line_id)

        run_by_trigger = {run.trigger_id: run for run in state.inference_runs if run.trigger_id}

        for item in state.items.values():
            trigger = state.triggers[item.trigger_id]
            run = run_by_trigger.get(item.trigger_id)
            cursor = conn.execute(
                """
                INSERT INTO inspection_item(
                    import_id, trigger_pk, trigger_id, material_id, camera_uuid,
                    receive_image_time, go_to_detect_time, detect_finish_time, result_received_time,
                    is_ok, result_raw, result_primary_code, defect_count_derived,
                    trigger_to_receive_ms, receive_to_detect_ms, to_detect_to_result_ms,
                    inference_run_id, link_method, link_confidence
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    import_id,
                    trigger.trigger_pk,
                    item.trigger_id,
                    item.material_id,
                    item.camera_uuid,
                    item.receive_image_time,
                    item.go_to_detect_time,
                    item.detect_finish_time,
                    item.result_received_time,
                    item.is_ok,
                    item.result_raw,
                    item.result_primary_code,
                    item.defect_count_derived,
                    item.trigger_to_receive_ms,
                    item.receive_to_detect_ms,
                    item.to_detect_to_result_ms,
                    run.run_id if run else None,
                    "trigger_id_camera_uuid",
                    1.0,
                ),
            )
            item.item_id = cursor.lastrowid
            stage = state.stage_by_key.get((item.trigger_id, item.camera_uuid))
            insert_evidence(conn, "inspection_item", item.item_id, "receive_image", stage.receive_line_id if stage else None)
            insert_evidence(conn, "inspection_item", item.item_id, "go_to_detect", stage.goto_line_id if stage else None)
            insert_evidence(conn, "inspection_item", item.item_id, "result_received", stage.result_line_id if stage else None)
            insert_evidence(conn, "inspection_item", item.item_id, "detect_done", item.source_detect_line_id)

            for defect_seq, token in enumerate(defect_tokens(item.result_raw), start=1):
                defect_row = conn.execute(
                    """
                    INSERT INTO defect_observation(item_id, defect_seq, defect_code, source_type, raw_token)
                    VALUES (?, ?, ?, 'summary_log', ?)
                    """,
                    (item.item_id, defect_seq, token, token),
                )
                insert_evidence(conn, "defect_observation", defect_row.lastrowid, "detect_done", item.source_detect_line_id)

        for event in state.equipment_events:
            if event.linked_trigger_id and event.linked_trigger_id in state.triggers:
                event.linked_trigger_pk = state.triggers[event.linked_trigger_id].trigger_pk
            event_row = conn.execute(
                """
                INSERT INTO equipment_event(
                    import_id, event_time, level, thread_id, source_symbol, event_category,
                    event_type, message, signal_value, db_no, offset_no, value,
                    linked_trigger_pk, link_method, link_confidence
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    import_id,
                    event.event_time,
                    event.level,
                    event.thread_id,
                    event.source_symbol,
                    event.event_category,
                    event.event_type,
                    event.message,
                    event.signal_value,
                    event.db_no,
                    event.offset_no,
                    event.value,
                    event.linked_trigger_pk,
                    event.link_method,
                    event.link_confidence,
                ),
            )
            event.event_id = event_row.lastrowid
            insert_evidence(conn, "equipment_event", event.event_id, "source", event.source_line_id)

    print_summary(conn, import_id, db_path)
    conn.close()


def scalar(conn: sqlite3.Connection, sql: str, params=()):
    return conn.execute(sql, params).fetchone()[0]


def print_summary(conn: sqlite3.Connection, import_id: int, db_path: str) -> None:
    print(f"database={db_path}")
    print(f"raw_log_line={scalar(conn, 'SELECT COUNT(*) FROM raw_log_line WHERE import_id = ?', (import_id,))}")
    print(f"inspection_trigger={scalar(conn, 'SELECT COUNT(*) FROM inspection_trigger WHERE import_id = ?', (import_id,))}")
    print(f"inference_run={scalar(conn, 'SELECT COUNT(*) FROM inference_run WHERE import_id = ?', (import_id,))}")
    print(f"inspection_item={scalar(conn, 'SELECT COUNT(*) FROM inspection_item WHERE import_id = ?', (import_id,))}")
    print(f"defect_observation={scalar(conn, 'SELECT COUNT(*) FROM defect_observation')}")
    print(f"equipment_event={scalar(conn, 'SELECT COUNT(*) FROM equipment_event WHERE import_id = ?', (import_id,))}")
    print(f"entity_evidence={scalar(conn, 'SELECT COUNT(*) FROM entity_evidence')}")
    print("\nresult_distribution")
    for row in conn.execute("SELECT result_raw, COUNT(*) FROM inspection_item GROUP BY result_raw ORDER BY COUNT(*) DESC"):
        print(f"  {row[0]}: {row[1]}")
    print("\ndefect_distribution")
    for row in conn.execute("SELECT defect_code, COUNT(*) FROM defect_observation GROUP BY defect_code ORDER BY COUNT(*) DESC"):
        print(f"  {row[0]}: {row[1]}")
    print("\ncamera_ng_rate")
    for row in conn.execute(
        """
        SELECT camera_id, COUNT(*), SUM(CASE WHEN is_ok = 0 THEN 1 ELSE 0 END),
               ROUND(100.0 * SUM(CASE WHEN is_ok = 0 THEN 1 ELSE 0 END) / COUNT(*), 2)
        FROM v_inspection_item_flat
        GROUP BY camera_id
        ORDER BY 4 DESC
        """
    ):
        print(f"  {row[0]}: total={row[1]}, ng={row[2]}, ng_rate={row[3]}%")


def main() -> None:
    parser = argparse.ArgumentParser(description="Import real VI log into SQLite semantic schema.")
    parser.add_argument("--log", default="app_20260613_180017.log")
    parser.add_argument("--db", default="database/real_log_vi_agent.db")
    parser.add_argument("--schema", default="database/real_log_schema_proposal.sql")
    parser.add_argument("--project-id", default="wuliangye_line_a")
    args = parser.parse_args()
    import_log(args)


if __name__ == "__main__":
    main()
