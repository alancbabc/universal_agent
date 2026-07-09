-- Deterministic test data for VI Agent development.
PRAGMA foreign_keys = ON;

INSERT INTO project_info (
    project_id,
    project_name,
    line_id,
    station_id,
    product_name,
    created_at
) VALUES (
    'wuliangye_line_a',
    '五粮液瓶盖缺陷检测测试项目',
    'line_a',
    'station_1',
    'bottle_cap',
    '2026-06-14 02:40:00.000'
);

INSERT INTO camera (camera_id, camera_name, station_no, position_name, enabled) VALUES
    ('DA8352810', '相机 1', 1, '左前', 1),
    ('DA8352821', '相机 2', 2, '右前', 1),
    ('DA8352848', '相机 3', 3, '左后', 1),
    ('DA8352862', '相机 4', 4, '右后', 1);

INSERT INTO model_version (
    model_version_id,
    model_name,
    model_version,
    confidence_threshold,
    deployed_at,
    remark
) VALUES
    (1, 'cap_defect_yolo', 'v1.8.2', 0.50, '2026-06-01 08:00:00.000', 'baseline'),
    (2, 'cap_defect_yolo', 'v1.8.3', 0.50, '2026-06-14 03:09:00.000', 'test rollout');

WITH RECURSIVE seq(n) AS (
    VALUES(0)
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 119
),
batch_rows AS (
    SELECT
        n,
        3334 + n AS trigger_id,
        (
            CASE WHEN n BETWEEN 10 AND 35 AND n % 3 = 0 THEN 1 ELSE 0 END +
            CASE WHEN n BETWEEN 40 AND 55 AND n % 4 = 0 THEN 1 ELSE 0 END +
            CASE WHEN n BETWEEN 70 AND 76 THEN 1 ELSE 0 END +
            CASE WHEN n % 41 = 0 THEN 1 ELSE 0 END
        ) AS ng_count
    FROM seq
)
INSERT INTO inspection_batch (
    trigger_id,
    product_sn,
    batch_time,
    plc_signal,
    line_id,
    station_id,
    expected_camera_count,
    batch_result,
    total_ng_count,
    cycle_ms
)
SELECT
    trigger_id,
    printf('SN%06d', trigger_id),
    strftime('%Y-%m-%d %H:%M:%f', julianday('2026-06-14 02:45:00') + (n * 18.0 / 86400.0)),
    (n % 4) + 1,
    'line_a',
    'station_1',
    4,
    CASE WHEN ng_count > 0 THEN 'NG' ELSE 'OK' END,
    ng_count,
    1180 + (n % 7) * 25
FROM batch_rows;

WITH item_source AS (
    SELECT
        b.batch_id,
        b.trigger_id,
        b.batch_time,
        b.trigger_id - 3334 AS n,
        c.camera_id,
        c.station_no
    FROM inspection_batch b
    CROSS JOIN camera c
),
classified AS (
    SELECT
        *,
        CASE
            WHEN camera_id = 'DA8352862' AND n BETWEEN 10 AND 35 AND n % 3 = 0 THEN 'HD'
            WHEN camera_id = 'DA8352848' AND n BETWEEN 40 AND 55 AND n % 4 = 0 THEN 'ZW'
            WHEN camera_id = 'DA8352810' AND n BETWEEN 70 AND 76 THEN 'HD'
            WHEN camera_id = 'DA8352821' AND n % 41 = 0 THEN 'QL'
            ELSE 'OK'
        END AS result_code
    FROM item_source
)
INSERT INTO inspection_item (
    batch_id,
    trigger_id,
    camera_id,
    capture_time,
    detect_time,
    is_ok,
    result_code,
    result_name,
    confidence,
    model_version_id,
    infer_ms,
    trigger_to_image_ms,
    receive_to_detect_ms,
    raw_result_text
)
SELECT
    batch_id,
    trigger_id,
    camera_id,
    strftime('%Y-%m-%d %H:%M:%f', julianday(batch_time) + ((145 + station_no * 3) / 1000.0 / 86400.0)),
    strftime('%Y-%m-%d %H:%M:%f', julianday(batch_time) + ((560 + (n % 8) * 28 + station_no * 5) / 1000.0 / 86400.0)),
    CASE WHEN result_code = 'OK' THEN 1 ELSE 0 END,
    result_code,
    CASE result_code
        WHEN 'OK' THEN '正常'
        WHEN 'HD' THEN '黑点'
        WHEN 'ZW' THEN '脏污'
        WHEN 'QL' THEN '缺料'
        ELSE '未知'
    END,
    CASE
        WHEN result_code = 'OK' THEN 0.96 - ((n + station_no) % 4) * 0.01
        WHEN result_code = 'HD' THEN 0.72 + (n % 5) * 0.04
        WHEN result_code = 'ZW' THEN 0.68 + (n % 4) * 0.05
        WHEN result_code = 'QL' THEN 0.61 + (n % 3) * 0.06
        ELSE 0.50
    END,
    CASE WHEN n < 80 THEN 1 ELSE 2 END,
    420 + (n % 9) * 36 + station_no * 7 + CASE WHEN n BETWEEN 60 AND 66 THEN 260 ELSE 0 END,
    148 + station_no * 2 + (n % 5),
    1 + (n + station_no) % 4,
    printf('相机 %s 触发ID %d 检测完成，isOK = %d, 结果 = %s',
        camera_id,
        trigger_id,
        CASE WHEN result_code = 'OK' THEN 1 ELSE 0 END,
        result_code)
FROM classified;

INSERT INTO image_asset (
    item_id,
    image_path,
    image_width,
    image_height,
    saved_at
)
SELECT
    item_id,
    printf('D:/Device/Images/%s/%d.jpg', camera_id, trigger_id),
    2448,
    2048,
    detect_time
FROM inspection_item;

INSERT INTO defect_instance (
    item_id,
    defect_code,
    defect_name,
    confidence,
    bbox_x,
    bbox_y,
    bbox_w,
    bbox_h,
    area,
    severity,
    model_version_id
)
SELECT
    item_id,
    result_code,
    result_name,
    confidence,
    CASE
        WHEN camera_id = 'DA8352862' THEN 1720 + (trigger_id % 9) * 14
        WHEN camera_id = 'DA8352848' THEN 520 + (trigger_id % 11) * 18
        WHEN camera_id = 'DA8352810' THEN 910 + (trigger_id % 7) * 20
        ELSE 1120 + (trigger_id % 5) * 16
    END,
    CASE
        WHEN camera_id = 'DA8352862' THEN 310 + (trigger_id % 6) * 15
        WHEN camera_id = 'DA8352848' THEN 1280 + (trigger_id % 8) * 12
        WHEN camera_id = 'DA8352810' THEN 780 + (trigger_id % 5) * 18
        ELSE 940 + (trigger_id % 4) * 14
    END,
    CASE result_code WHEN 'QL' THEN 110 ELSE 46 + (trigger_id % 6) * 5 END,
    CASE result_code WHEN 'QL' THEN 90 ELSE 38 + (trigger_id % 5) * 4 END,
    CASE result_code WHEN 'QL' THEN 9900 ELSE (46 + (trigger_id % 6) * 5) * (38 + (trigger_id % 5) * 4) END,
    CASE
        WHEN confidence >= 0.85 THEN 'high'
        WHEN confidence >= 0.70 THEN 'middle'
        ELSE 'low'
    END,
    model_version_id
FROM inspection_item
WHERE is_ok = 0;

INSERT INTO runtime_event (
    event_time,
    trigger_id,
    camera_id,
    level,
    event_type,
    source,
    thread_id,
    message,
    duration_ms,
    metadata_json
)
SELECT
    batch_time,
    trigger_id,
    NULL,
    'INFO',
    'plc_trigger',
    'PLCClient::BridgeDataChanged',
    4456,
    printf('[PLCClinent] BridgeDataChanged: %d', plc_signal),
    NULL,
    json_object('plc_signal', plc_signal)
FROM inspection_batch;

INSERT INTO runtime_event (
    event_time,
    trigger_id,
    camera_id,
    level,
    event_type,
    source,
    thread_id,
    message,
    duration_ms,
    metadata_json
)
SELECT
    capture_time,
    trigger_id,
    camera_id,
    'DEBUG',
    'capture_finished',
    'CameraData::setReceiveImage',
    12452,
    printf('相机 %s, 物料 %d, 触发->收图 %dms', camera_id, trigger_id, trigger_to_image_ms),
    trigger_to_image_ms,
    json_object('stage', 'trigger_to_image')
FROM inspection_item;

INSERT INTO runtime_event (
    event_time,
    trigger_id,
    camera_id,
    level,
    event_type,
    source,
    thread_id,
    message,
    duration_ms,
    metadata_json
)
SELECT
    detect_time,
    trigger_id,
    camera_id,
    'INFO',
    'detect_finished',
    'YOLODetector::doWork',
    9400,
    raw_result_text,
    infer_ms,
    json_object('result_code', result_code, 'is_ok', is_ok, 'confidence', confidence)
FROM inspection_item;

INSERT INTO runtime_event (
    event_time,
    trigger_id,
    camera_id,
    level,
    event_type,
    source,
    thread_id,
    message,
    duration_ms,
    metadata_json
)
SELECT
    batch_time,
    trigger_id,
    NULL,
    'WARN',
    'batch_cache_reset',
    'WuLiangYeProcess::doWork',
    12452,
    '批次缓存结果重置',
    NULL,
    json_object('reason', 'test warning injected')
FROM inspection_batch
WHERE trigger_id IN (3334 + 75, 3334 + 110);

