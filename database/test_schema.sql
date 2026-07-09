-- Test-only SQLite schema for VI Agent development.
PRAGMA foreign_keys = ON;

DROP VIEW IF EXISTS v_defect_instance_flat;
DROP VIEW IF EXISTS v_inspection_item_flat;
DROP TABLE IF EXISTS runtime_event;
DROP TABLE IF EXISTS defect_instance;
DROP TABLE IF EXISTS image_asset;
DROP TABLE IF EXISTS inspection_item;
DROP TABLE IF EXISTS inspection_batch;
DROP TABLE IF EXISTS model_version;
DROP TABLE IF EXISTS camera;
DROP TABLE IF EXISTS project_info;

CREATE TABLE project_info (
    project_id TEXT PRIMARY KEY,
    project_name TEXT NOT NULL,
    line_id TEXT NOT NULL,
    station_id TEXT NOT NULL,
    product_name TEXT,
    created_at TEXT NOT NULL
);

CREATE TABLE camera (
    camera_id TEXT PRIMARY KEY,
    camera_name TEXT NOT NULL,
    station_no INTEGER NOT NULL,
    position_name TEXT NOT NULL,
    enabled INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE model_version (
    model_version_id INTEGER PRIMARY KEY,
    model_name TEXT NOT NULL,
    model_version TEXT NOT NULL,
    confidence_threshold REAL NOT NULL,
    deployed_at TEXT NOT NULL,
    remark TEXT
);

CREATE TABLE inspection_batch (
    batch_id INTEGER PRIMARY KEY AUTOINCREMENT,
    trigger_id INTEGER NOT NULL UNIQUE,
    product_sn TEXT,
    batch_time TEXT NOT NULL,
    plc_signal INTEGER,
    line_id TEXT,
    station_id TEXT,
    expected_camera_count INTEGER NOT NULL DEFAULT 4,
    batch_result TEXT NOT NULL,
    total_ng_count INTEGER NOT NULL DEFAULT 0,
    cycle_ms INTEGER
);

CREATE TABLE inspection_item (
    item_id INTEGER PRIMARY KEY AUTOINCREMENT,
    batch_id INTEGER NOT NULL,
    trigger_id INTEGER NOT NULL,
    camera_id TEXT NOT NULL,
    capture_time TEXT NOT NULL,
    detect_time TEXT NOT NULL,
    is_ok INTEGER NOT NULL,
    result_code TEXT NOT NULL,
    result_name TEXT NOT NULL,
    confidence REAL,
    model_version_id INTEGER NOT NULL,
    infer_ms INTEGER,
    trigger_to_image_ms INTEGER,
    receive_to_detect_ms INTEGER,
    raw_result_text TEXT,
    FOREIGN KEY (batch_id) REFERENCES inspection_batch(batch_id),
    FOREIGN KEY (camera_id) REFERENCES camera(camera_id),
    FOREIGN KEY (model_version_id) REFERENCES model_version(model_version_id),
    UNIQUE (batch_id, camera_id)
);

CREATE TABLE image_asset (
    image_id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER NOT NULL UNIQUE,
    image_path TEXT NOT NULL,
    image_width INTEGER,
    image_height INTEGER,
    saved_at TEXT,
    FOREIGN KEY (item_id) REFERENCES inspection_item(item_id)
);

CREATE TABLE defect_instance (
    defect_id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER NOT NULL,
    defect_code TEXT NOT NULL,
    defect_name TEXT NOT NULL,
    confidence REAL NOT NULL,
    bbox_x REAL NOT NULL,
    bbox_y REAL NOT NULL,
    bbox_w REAL NOT NULL,
    bbox_h REAL NOT NULL,
    area REAL,
    severity TEXT,
    model_version_id INTEGER NOT NULL,
    FOREIGN KEY (item_id) REFERENCES inspection_item(item_id),
    FOREIGN KEY (model_version_id) REFERENCES model_version(model_version_id)
);

CREATE TABLE runtime_event (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_time TEXT NOT NULL,
    trigger_id INTEGER,
    camera_id TEXT,
    level TEXT NOT NULL,
    event_type TEXT NOT NULL,
    source TEXT,
    thread_id INTEGER,
    message TEXT NOT NULL,
    duration_ms INTEGER,
    metadata_json TEXT
);

CREATE INDEX idx_batch_time ON inspection_batch(batch_time);
CREATE INDEX idx_item_detect_time ON inspection_item(detect_time);
CREATE INDEX idx_item_camera_time ON inspection_item(camera_id, detect_time);
CREATE INDEX idx_item_result ON inspection_item(is_ok, result_code);
CREATE INDEX idx_defect_item ON defect_instance(item_id);
CREATE INDEX idx_defect_code ON defect_instance(defect_code);
CREATE INDEX idx_event_time_type ON runtime_event(event_time, event_type);

CREATE VIEW v_inspection_item_flat AS
SELECT
    b.batch_id,
    b.trigger_id,
    b.product_sn,
    b.batch_time,
    b.batch_result,
    b.total_ng_count,
    i.item_id,
    i.camera_id,
    c.camera_name,
    c.position_name,
    i.capture_time,
    i.detect_time,
    i.is_ok,
    i.result_code,
    i.result_name,
    i.confidence,
    i.infer_ms,
    i.trigger_to_image_ms,
    i.receive_to_detect_ms,
    mv.model_name,
    mv.model_version,
    img.image_path,
    img.image_width,
    img.image_height
FROM inspection_item i
JOIN inspection_batch b ON b.batch_id = i.batch_id
JOIN camera c ON c.camera_id = i.camera_id
JOIN model_version mv ON mv.model_version_id = i.model_version_id
LEFT JOIN image_asset img ON img.item_id = i.item_id;

CREATE VIEW v_defect_instance_flat AS
SELECT
    d.defect_id,
    d.item_id,
    i.trigger_id,
    i.camera_id,
    i.detect_time,
    d.defect_code,
    d.defect_name,
    d.confidence,
    d.bbox_x,
    d.bbox_y,
    d.bbox_w,
    d.bbox_h,
    d.area,
    d.severity,
    img.image_path,
    mv.model_version
FROM defect_instance d
JOIN inspection_item i ON i.item_id = d.item_id
JOIN model_version mv ON mv.model_version_id = d.model_version_id
LEFT JOIN image_asset img ON img.item_id = i.item_id;

