PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS log_import (
    import_id INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id TEXT NOT NULL,
    file_path TEXT NOT NULL,
    file_name TEXT NOT NULL,
    file_size INTEGER,
    sha256 TEXT,
    schema_version TEXT NOT NULL DEFAULT 'real_log_v0.2',
    parser_version TEXT NOT NULL DEFAULT 'log_importer_v0.2',
    parse_rule_version TEXT NOT NULL DEFAULT 'wuliangye_log_rules_v0.2',
    first_event_time TEXT,
    last_event_time TEXT,
    imported_at TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(file_path, sha256)
);

CREATE TABLE IF NOT EXISTS raw_log_line (
    line_id INTEGER PRIMARY KEY AUTOINCREMENT,
    import_id INTEGER NOT NULL,
    line_no INTEGER NOT NULL,
    event_time TEXT,
    level TEXT,
    thread_id INTEGER,
    source_symbol TEXT,
    class_name TEXT,
    function_name TEXT,
    source_line_no INTEGER,
    message TEXT,
    event_type TEXT,
    parse_status TEXT NOT NULL DEFAULT 'parsed_header',
    raw_text TEXT NOT NULL,
    FOREIGN KEY(import_id) REFERENCES log_import(import_id),
    UNIQUE(import_id, line_no)
);

CREATE TABLE IF NOT EXISTS camera (
    camera_uuid TEXT PRIMARY KEY,
    camera_name TEXT,
    station_no INTEGER,
    position_name TEXT,
    enabled INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS inspection_trigger (
    trigger_pk INTEGER PRIMARY KEY AUTOINCREMENT,
    import_id INTEGER NOT NULL,
    trigger_id TEXT NOT NULL,
    material_id TEXT,
    trigger_seq INTEGER,
    plc_signal_value INTEGER,
    bridge_event_time TEXT,
    trigger_all_camera_time TEXT,
    first_receive_time TEXT,
    first_detect_finish_time TEXT,
    expected_camera_count INTEGER NOT NULL DEFAULT 4,
    actual_camera_count INTEGER,
    status TEXT,
    link_method TEXT,
    link_confidence REAL,
    FOREIGN KEY(import_id) REFERENCES log_import(import_id),
    UNIQUE(import_id, trigger_id)
);

CREATE TABLE IF NOT EXISTS inference_run (
    run_id INTEGER PRIMARY KEY AUTOINCREMENT,
    import_id INTEGER NOT NULL,
    trigger_pk INTEGER,
    precheck_time TEXT,
    input_image_count INTEGER,
    start_time TEXT,
    end_time TEXT,
    output_result_count INTEGER,
    infer_ms INTEGER,
    thread_id INTEGER,
    link_method TEXT,
    link_confidence REAL,
    FOREIGN KEY(import_id) REFERENCES log_import(import_id),
    FOREIGN KEY(trigger_pk) REFERENCES inspection_trigger(trigger_pk)
);

CREATE TABLE IF NOT EXISTS inspection_item (
    item_id INTEGER PRIMARY KEY AUTOINCREMENT,
    import_id INTEGER NOT NULL,
    trigger_pk INTEGER NOT NULL,
    trigger_id TEXT NOT NULL,
    material_id TEXT,
    camera_uuid TEXT NOT NULL,
    receive_image_time TEXT,
    go_to_detect_time TEXT,
    detect_finish_time TEXT,
    result_received_time TEXT,
    is_ok INTEGER,
    result_raw TEXT,
    result_primary_code TEXT,
    defect_count_derived INTEGER,
    trigger_to_receive_ms INTEGER,
    receive_to_detect_ms INTEGER,
    to_detect_to_result_ms INTEGER,
    inference_run_id INTEGER,
    link_method TEXT,
    link_confidence REAL,
    FOREIGN KEY(import_id) REFERENCES log_import(import_id),
    FOREIGN KEY(trigger_pk) REFERENCES inspection_trigger(trigger_pk),
    FOREIGN KEY(camera_uuid) REFERENCES camera(camera_uuid),
    FOREIGN KEY(inference_run_id) REFERENCES inference_run(run_id),
    UNIQUE(import_id, trigger_id, camera_uuid)
);

CREATE TABLE IF NOT EXISTS defect_observation (
    defect_id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER NOT NULL,
    defect_seq INTEGER NOT NULL,
    defect_code TEXT NOT NULL,
    defect_name TEXT,
    confidence REAL,
    bbox_x REAL,
    bbox_y REAL,
    bbox_w REAL,
    bbox_h REAL,
    area REAL,
    image_path TEXT,
    model_version TEXT,
    source_type TEXT NOT NULL DEFAULT 'summary_log',
    raw_token TEXT,
    FOREIGN KEY(item_id) REFERENCES inspection_item(item_id),
    UNIQUE(item_id, defect_seq)
);

CREATE TABLE IF NOT EXISTS equipment_event (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    import_id INTEGER NOT NULL,
    event_time TEXT NOT NULL,
    level TEXT,
    thread_id INTEGER,
    source_symbol TEXT,
    event_category TEXT NOT NULL,
    event_type TEXT NOT NULL,
    message TEXT NOT NULL,
    signal_value INTEGER,
    db_no INTEGER,
    offset_no INTEGER,
    value INTEGER,
    linked_trigger_pk INTEGER,
    link_method TEXT,
    link_confidence REAL,
    FOREIGN KEY(import_id) REFERENCES log_import(import_id),
    FOREIGN KEY(linked_trigger_pk) REFERENCES inspection_trigger(trigger_pk)
);

CREATE TABLE IF NOT EXISTS entity_evidence (
    evidence_id INTEGER PRIMARY KEY AUTOINCREMENT,
    entity_type TEXT NOT NULL,
    entity_id INTEGER NOT NULL,
    evidence_role TEXT NOT NULL,
    line_id INTEGER NOT NULL,
    FOREIGN KEY(line_id) REFERENCES raw_log_line(line_id),
    UNIQUE(entity_type, entity_id, evidence_role, line_id)
);

CREATE TABLE IF NOT EXISTS defect_dictionary (
    defect_code TEXT PRIMARY KEY,
    defect_name TEXT NOT NULL,
    is_ok_code INTEGER NOT NULL DEFAULT 0
);

INSERT OR IGNORE INTO defect_dictionary(defect_code, defect_name, is_ok_code) VALUES
('OK', '正常', 1),
('HD', '黑点', 0),
('ZW', '脏污', 0),
('QL', '缺料', 0);

CREATE INDEX IF NOT EXISTS idx_raw_log_line_time ON raw_log_line(event_time);
CREATE INDEX IF NOT EXISTS idx_raw_log_line_type ON raw_log_line(event_type);
CREATE INDEX IF NOT EXISTS idx_trigger_import_id ON inspection_trigger(import_id, trigger_id);
CREATE INDEX IF NOT EXISTS idx_trigger_time ON inspection_trigger(trigger_all_camera_time);
CREATE INDEX IF NOT EXISTS idx_item_trigger ON inspection_item(trigger_pk);
CREATE INDEX IF NOT EXISTS idx_item_detect_time ON inspection_item(detect_finish_time);
CREATE INDEX IF NOT EXISTS idx_item_camera_time ON inspection_item(camera_uuid, detect_finish_time);
CREATE INDEX IF NOT EXISTS idx_item_time_result ON inspection_item(detect_finish_time, is_ok, result_primary_code);
CREATE INDEX IF NOT EXISTS idx_defect_item ON defect_observation(item_id);
CREATE INDEX IF NOT EXISTS idx_defect_code ON defect_observation(defect_code);
CREATE INDEX IF NOT EXISTS idx_infer_trigger ON inference_run(trigger_pk);
CREATE INDEX IF NOT EXISTS idx_infer_time ON inference_run(start_time, end_time);
CREATE INDEX IF NOT EXISTS idx_equipment_event_time ON equipment_event(event_time);
CREATE INDEX IF NOT EXISTS idx_equipment_event_type ON equipment_event(event_time, event_type);
CREATE INDEX IF NOT EXISTS idx_equipment_event_trigger ON equipment_event(linked_trigger_pk);
CREATE INDEX IF NOT EXISTS idx_entity_evidence_entity ON entity_evidence(entity_type, entity_id);

CREATE VIEW IF NOT EXISTS v_inspection_item_flat AS
SELECT
    i.item_id,
    i.trigger_pk,
    i.trigger_id,
    i.material_id,
    COALESCE(i.detect_finish_time, i.result_received_time, i.receive_image_time) AS detect_time,
    i.camera_uuid AS camera_id,
    COALESCE(c.camera_name, i.camera_uuid) AS camera_name,
    i.is_ok,
    i.result_primary_code AS result_code,
    COALESCE(d.defect_name, i.result_primary_code) AS result_name,
    i.result_raw,
    i.defect_count_derived AS defect_count,
    r.infer_ms,
    i.trigger_to_receive_ms,
    i.receive_to_detect_ms,
    i.to_detect_to_result_ms,
    NULL AS image_path,
    NULL AS model_version
FROM inspection_item i
LEFT JOIN camera c ON c.camera_uuid = i.camera_uuid
LEFT JOIN defect_dictionary d ON d.defect_code = i.result_primary_code
LEFT JOIN inference_run r ON r.run_id = i.inference_run_id;

CREATE VIEW IF NOT EXISTS v_defect_instance_flat AS
SELECT
    od.defect_id,
    od.item_id,
    i.trigger_pk,
    i.trigger_id,
    COALESCE(i.detect_finish_time, i.result_received_time, i.receive_image_time) AS detect_time,
    i.camera_uuid AS camera_id,
    od.defect_code,
    COALESCE(od.defect_name, d.defect_name, od.defect_code) AS defect_name,
    od.confidence,
    od.bbox_x,
    od.bbox_y,
    od.bbox_w,
    od.bbox_h,
    od.area,
    od.image_path,
    od.model_version
FROM defect_observation od
JOIN inspection_item i ON i.item_id = od.item_id
LEFT JOIN defect_dictionary d ON d.defect_code = od.defect_code;

CREATE VIEW IF NOT EXISTS v_equipment_event_flat AS
SELECT
    e.event_id,
    e.event_time,
    e.level,
    e.event_category,
    e.event_type,
    e.source_symbol,
    e.message,
    e.signal_value,
    e.db_no,
    e.offset_no,
    e.value,
    t.trigger_id,
    e.link_method,
    e.link_confidence
FROM equipment_event e
LEFT JOIN inspection_trigger t ON t.trigger_pk = e.linked_trigger_pk;

CREATE VIEW IF NOT EXISTS v_runtime_event_flat AS
SELECT
    event_id,
    event_time,
    level,
    event_type,
    source_symbol,
    message,
    trigger_id,
    NULL AS camera_id
FROM v_equipment_event_flat
WHERE level IN ('WARN', 'ERROR') OR event_category IN ('SYSTEM', 'PROCESS');

CREATE VIEW IF NOT EXISTS v_trigger_summary AS
SELECT
    t.trigger_pk,
    t.trigger_id,
    t.material_id,
    t.trigger_all_camera_time,
    t.expected_camera_count,
    COUNT(i.item_id) AS item_count,
    SUM(CASE WHEN i.is_ok = 0 THEN 1 ELSE 0 END) AS ng_item_count,
    AVG(i.trigger_to_receive_ms) AS avg_trigger_to_receive_ms,
    AVG(i.receive_to_detect_ms) AS avg_receive_to_detect_ms,
    AVG(i.to_detect_to_result_ms) AS avg_to_detect_to_result_ms,
    MAX(r.infer_ms) AS infer_ms,
    SUM(CASE WHEN e.level IN ('WARN', 'ERROR') THEN 1 ELSE 0 END) AS warn_error_count
FROM inspection_trigger t
LEFT JOIN inspection_item i ON i.trigger_pk = t.trigger_pk
LEFT JOIN inference_run r ON r.trigger_pk = t.trigger_pk
LEFT JOIN equipment_event e ON e.linked_trigger_pk = t.trigger_pk
GROUP BY t.trigger_pk;

CREATE VIEW IF NOT EXISTS v_anomaly_context AS
SELECT
    i.item_id,
    i.trigger_pk,
    i.trigger_id,
    i.material_id,
    i.detect_time,
    i.camera_id,
    i.camera_name,
    i.is_ok,
    i.result_code,
    i.result_name,
    i.result_raw,
    i.defect_count,
    i.infer_ms,
    i.trigger_to_receive_ms,
    i.receive_to_detect_ms,
    i.to_detect_to_result_ms,
    ts.item_count AS trigger_item_count,
    ts.ng_item_count AS trigger_ng_item_count,
    ts.warn_error_count AS trigger_warn_error_count
FROM v_inspection_item_flat i
LEFT JOIN v_trigger_summary ts ON ts.trigger_pk = i.trigger_pk;