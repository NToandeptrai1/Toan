-- Tạo bảng pending_commands
CREATE TABLE IF NOT EXISTS pending_commands (
    id BIGSERIAL PRIMARY KEY,
    command VARCHAR(50) NOT NULL,
    executed BOOLEAN DEFAULT false,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Tạo index để truy vấn nhanh
CREATE INDEX idx_pending_executed ON pending_commands(executed, created_at);

-- Function tự động xóa lệnh cũ sau 1 giờ
CREATE OR REPLACE FUNCTION cleanup_old_commands()
RETURNS TRIGGER AS $$
BEGIN
    DELETE FROM pending_commands
    WHERE created_at < NOW() - INTERVAL '1 hour';
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Trigger để chạy function trên
CREATE TRIGGER trigger_cleanup_commands
AFTER INSERT ON pending_commands
EXECUTE FUNCTION cleanup_old_commands();