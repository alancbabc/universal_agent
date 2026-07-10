import bisect
import importlib.util
import os
from datetime import datetime


BASE_SCRIPT = os.path.join(os.path.dirname(__file__), "import_real_log.py")


def load_base_module():
    spec = importlib.util.spec_from_file_location("import_real_log_base", BASE_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    if spec.loader is None:
        raise RuntimeError("failed to load import_real_log.py")
    spec.loader.exec_module(module)
    return module


_trigger_index_cache = {}


def fast_nearest_trigger_by_time(triggers, event_time: str, max_seconds: float):
    cache_key = id(triggers)
    cached = _trigger_index_cache.get(cache_key)
    if cached is None:
        pairs = []
        for trigger in triggers:
            anchor = trigger.trigger_all_camera_time or trigger.first_receive_time or trigger.first_detect_finish_time
            if anchor:
                pairs.append((datetime.strptime(anchor, "%Y-%m-%d %H:%M:%S.%f"), trigger))
        pairs.sort(key=lambda item: item[0])
        cached = ([item[0] for item in pairs], [item[1] for item in pairs])
        _trigger_index_cache[cache_key] = cached

    trigger_times, indexed_triggers = cached
    if not trigger_times:
        return None

    target = datetime.strptime(event_time, "%Y-%m-%d %H:%M:%S.%f")
    pos = bisect.bisect_left(trigger_times, target)
    candidates = []
    for index in (pos - 1, pos):
        if 0 <= index < len(trigger_times):
            delta = abs((target - trigger_times[index]).total_seconds())
            if delta <= max_seconds:
                candidates.append((delta, indexed_triggers[index]))
    if not candidates:
        return None
    candidates.sort(key=lambda item: item[0])
    return candidates[0][1]


def main():
    module = load_base_module()
    module.nearest_trigger_by_time = fast_nearest_trigger_by_time
    module.main()


if __name__ == "__main__":
    main()
