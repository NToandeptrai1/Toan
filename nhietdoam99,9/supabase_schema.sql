-- =============================================
-- IRRIGATION SYSTEM DATABASE SCHEMA
-- =============================================

-- 1. Bảng lưu dữ liệu cảm biến (time-series data)
CREATE TABLE IF NOT EXISTS sensor_data (
    id BIGSERIAL PRIMARY KEY,
    moisture INTEGER NOT NULL,
    status VARCHAR(20) NOT NULL,
    auto_mode BOOLEAN NOT NULL DEFAULT true,
    pump_on BOOLEAN NOT NULL DEFAULT false,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Index để query nhanh theo thời gian
CREATE INDEX idx_sensor_data_created_at ON sensor_data(created_at DESC);

-- 2. Bảng lưu lịch sử điều khiển
CREATE TABLE IF NOT EXISTS control_logs (
    id BIGSERIAL PRIMARY KEY,
    command VARCHAR(50) NOT NULL,
    source VARCHAR(20) NOT NULL, -- 'web', 'mobile', 'auto', 'button'
    user_id UUID,
    executed_at TIMESTAMPTZ DEFAULT NOW()
);

-- Index
CREATE INDEX idx_control_logs_executed_at ON control_logs(executed_at DESC);

-- 3. Bảng cấu hình hệ thống
CREATE TABLE IF NOT EXISTS system_config (
    id INTEGER PRIMARY KEY DEFAULT 1,
    dry_threshold INTEGER DEFAULT 3000,
    critical_threshold INTEGER DEFAULT 3800,
    data_send_interval INTEGER DEFAULT 2000,
    auto_mode_enabled BOOLEAN DEFAULT true,
    notification_enabled BOOLEAN DEFAULT true,
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    CONSTRAINT single_row CHECK (id = 1)
);

-- Insert config mặc định
INSERT INTO system_config (id) VALUES (1) 
ON CONFLICT (id) DO NOTHING;

-- 4. Bảng thống kê theo ngày (để dashboard load nhanh)
CREATE TABLE IF NOT EXISTS daily_stats (
    id BIGSERIAL PRIMARY KEY,
    date DATE NOT NULL UNIQUE,
    avg_moisture DECIMAL(10,2),
    min_moisture INTEGER,
    max_moisture INTEGER,
    total_watering_time INTEGER, -- phút
    watering_count INTEGER DEFAULT 0,
    critical_events INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Index
CREATE INDEX idx_daily_stats_date ON daily_stats(date DESC);

-- 5. Bảng trạng thái thiết bị
CREATE TABLE IF NOT EXISTS device_status (
    id INTEGER PRIMARY KEY DEFAULT 1,
    device_name VARCHAR(50) DEFAULT 'ESP32',
    is_online BOOLEAN DEFAULT false,
    ip_address VARCHAR(15),
    last_seen TIMESTAMPTZ,
    firmware_version VARCHAR(20),
    CONSTRAINT single_device CHECK (id = 1)
);

-- Insert device mặc định
INSERT INTO device_status (id) VALUES (1)
ON CONFLICT (id) DO NOTHING;

-- =============================================
-- FUNCTIONS & TRIGGERS
-- =============================================

-- Function: Tự động tính daily stats
CREATE OR REPLACE FUNCTION update_daily_stats()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO daily_stats (
        date,
        avg_moisture,
        min_moisture,
        max_moisture,
        total_watering_time,
        watering_count,
        critical_events
    )
    SELECT
        DATE(created_at) as date,
        AVG(moisture)::DECIMAL(10,2),
        MIN(moisture),
        MAX(moisture),
        COUNT(CASE WHEN pump_on = true THEN 1 END) * 2 / 60, -- 2s interval -> minutes
        COUNT(CASE WHEN status = 'watering' THEN 1 END),
        COUNT(CASE WHEN status = 'critical' THEN 1 END)
    FROM sensor_data
    WHERE DATE(created_at) = CURRENT_DATE
    GROUP BY DATE(created_at)
    ON CONFLICT (date) 
    DO UPDATE SET
        avg_moisture = EXCLUDED.avg_moisture,
        min_moisture = EXCLUDED.min_moisture,
        max_moisture = EXCLUDED.max_moisture,
        total_watering_time = EXCLUDED.total_watering_time,
        watering_count = EXCLUDED.watering_count,
        critical_events = EXCLUDED.critical_events;
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Trigger: Chạy sau mỗi 100 records mới
CREATE OR REPLACE FUNCTION trigger_daily_stats_update()
RETURNS TRIGGER AS $$
BEGIN
    IF (SELECT COUNT(*) FROM sensor_data WHERE DATE(created_at) = CURRENT_DATE) % 100 = 0 THEN
        PERFORM update_daily_stats();
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER daily_stats_trigger
AFTER INSERT ON sensor_data
FOR EACH ROW
EXECUTE FUNCTION trigger_daily_stats_update();

-- Function: Xóa dữ liệu cũ (giữ 30 ngày)
CREATE OR REPLACE FUNCTION cleanup_old_data()
RETURNS void AS $$
BEGIN
    DELETE FROM sensor_data 
    WHERE created_at < NOW() - INTERVAL '30 days';
    
    DELETE FROM control_logs 
    WHERE executed_at < NOW() - INTERVAL '90 days';
END;
$$ LANGUAGE plpgsql;

-- =============================================
-- ROW LEVEL SECURITY (Optional - nếu cần auth)
-- =============================================

-- Enable RLS
ALTER TABLE sensor_data ENABLE ROW LEVEL SECURITY;
ALTER TABLE control_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE system_config ENABLE ROW LEVEL SECURITY;
ALTER TABLE daily_stats ENABLE ROW LEVEL SECURITY;
ALTER TABLE device_status ENABLE ROW LEVEL SECURITY;

-- Policy: Allow read cho mọi user authenticated
CREATE POLICY "Allow read sensor_data"
ON sensor_data FOR SELECT
TO authenticated
USING (true);

-- Policy: Allow insert cho service role (Python app)
CREATE POLICY "Allow insert sensor_data"
ON sensor_data FOR INSERT
TO service_role
WITH CHECK (true);

-- Tương tự cho các bảng khác
CREATE POLICY "Allow all control_logs"
ON control_logs FOR ALL
TO authenticated
USING (true);

CREATE POLICY "Allow read system_config"
ON system_config FOR SELECT
TO authenticated
USING (true);

CREATE POLICY "Allow update system_config"
ON system_config FOR UPDATE
TO authenticated
USING (true);

CREATE POLICY "Allow read daily_stats"
ON daily_stats FOR SELECT
TO authenticated
USING (true);

CREATE POLICY "Allow read device_status"
ON device_status FOR SELECT
TO authenticated
USING (true);

-- =============================================
-- VIEWS (để query dễ hơn)
-- =============================================

-- View: Latest sensor reading
CREATE OR REPLACE VIEW latest_sensor_data AS
SELECT *
FROM sensor_data
ORDER BY created_at DESC
LIMIT 1;

-- View: Last 24 hours data
CREATE OR REPLACE VIEW sensor_data_24h AS
SELECT *
FROM sensor_data
WHERE created_at > NOW() - INTERVAL '24 hours'
ORDER BY created_at DESC;

-- View: Recent control logs
CREATE OR REPLACE VIEW recent_controls AS
SELECT *
FROM control_logs
ORDER BY executed_at DESC
LIMIT 50;

-- =============================================
-- SAMPLE QUERIES (để test)
-- =============================================

-- Test insert
-- INSERT INTO sensor_data (moisture, status, auto_mode, pump_on) 
-- VALUES (2500, 'ok', true, false);

-- Lấy dữ liệu 1 giờ gần nhất
-- SELECT * FROM sensor_data 
-- WHERE created_at > NOW() - INTERVAL '1 hour'
-- ORDER BY created_at DESC;

-- Thống kê 7 ngày
-- SELECT * FROM daily_stats 
-- WHERE date > CURRENT_DATE - INTERVAL '7 days'
-- ORDER BY date DESC;

-- Cleanup manual (chạy định kỳ bằng cron job)
-- SELECT cleanup_old_data();