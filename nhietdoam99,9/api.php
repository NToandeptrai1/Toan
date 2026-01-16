<?php
// ==============================================
// IRRIGATION SYSTEM - PHP API
// ==============================================
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

// Handle OPTIONS request
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

// ==============================================
// SUPABASE CONFIGURATION
// ==============================================
define('SUPABASE_URL', 'https://kiewjweubjlzmemnslor.supabase.co');
define('SUPABASE_KEY', 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImtpZXdqd2V1Ympsem1lbW5zbG9yIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTk0NTYzNzEsImV4cCI6MjA3NTAzMjM3MX0.m_c1dW_MkF6V7KmY9RHDfPGwjPmVYYV8F30rCjkxU5A');

// ==============================================
// HELPER FUNCTIONS
// ==============================================

function supabaseRequest($endpoint, $method = 'GET', $data = null) {
    $url = SUPABASE_URL . '/rest/v1/' . $endpoint;
    
    $headers = [
        'apikey: ' . SUPABASE_KEY,
        'Authorization: Bearer ' . SUPABASE_KEY,
        'Content-Type: application/json',
        'Prefer: return=representation'
    ];
    
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, $headers);
    
    if ($method === 'POST') {
        curl_setopt($ch, CURLOPT_POST, true);
        curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($data));
    } elseif ($method === 'PUT') {
        curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'PATCH');
        curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($data));
    }
    
    $response = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    
    if ($httpCode >= 200 && $httpCode < 300) {
        return json_decode($response, true);
    }
    
    return false;
}

function sendJsonResponse($success, $data = null, $error = null) {
    echo json_encode([
        'success' => $success,
        'data' => $data,
        'error' => $error,
        'timestamp' => date('c')
    ]);
    exit();
}

// ==============================================
// ROUTING
// ==============================================

$requestUri = $_SERVER['REQUEST_URI'];
$requestMethod = $_SERVER['REQUEST_METHOD'];

// Parse route
$path = parse_url($requestUri, PHP_URL_PATH);
$path = str_replace('/api.php', '', $path);
$path = trim($path, '/');

// ==============================================
// ROUTE: POST /data (ESP32 gửi dữ liệu)
// ==============================================
if ($path === 'data' && $requestMethod === 'POST') {
    $input = json_decode(file_get_contents('php://input'), true);
    
    if (!$input || !isset($input['moisture'])) {
        sendJsonResponse(false, null, 'Invalid data');
    }
    
    $data = [
        'moisture' => (int)$input['moisture'],
        'status' => $input['status'] ?? 'unknown',
        'auto_mode' => $input['autoMode'] ?? true,
        'pump_on' => $input['pumpOn'] ?? false
    ];
    
    $result = supabaseRequest('sensor_data_raw', 'POST', $data);
    
    if ($result) {
        sendJsonResponse(true, $result);
    } else {
        sendJsonResponse(false, null, 'Failed to save data');
    }
}

// ==============================================
// ROUTE: GET /status (Lấy trạng thái mới nhất)
// ==============================================
elseif ($path === 'status' && $requestMethod === 'GET') {
    $result = supabaseRequest('sensor_data_raw?order=created_at.desc&limit=1');
    
    if ($result && count($result) > 0) {
        $latest = $result[0];
        sendJsonResponse(true, [
            'moisture' => $latest['moisture'],
            'status' => $latest['status'],
            'autoMode' => $latest['auto_mode'],
            'pumpOn' => $latest['pump_on'],
            'timestamp' => $latest['created_at'],
            'connected' => true
        ]);
    } else {
        sendJsonResponse(true, [
            'moisture' => 0,
            'status' => 'unknown',
            'autoMode' => true,
            'pumpOn' => false,
            'timestamp' => null,
            'connected' => false
        ]);
    }
}

// ==============================================
// ROUTE: GET /history (Lịch sử dữ liệu)
// ==============================================
elseif ($path === 'history' && $requestMethod === 'GET') {
    $hours = $_GET['hours'] ?? 24;
    $limit = $_GET['limit'] ?? 1000;
    
    $timeThreshold = date('c', strtotime("-{$hours} hours"));
    
    $result = supabaseRequest(
        "sensor_data_raw?created_at=gte.{$timeThreshold}&order=created_at.desc&limit={$limit}"
    );
    
    if ($result !== false) {
        sendJsonResponse(true, $result);
    } else {
        sendJsonResponse(false, null, 'Failed to fetch history');
    }
}

// ==============================================
// ROUTE: GET /logs (Lịch sử điều khiển)
// ==============================================
elseif ($path === 'logs' && $requestMethod === 'GET') {
    $limit = $_GET['limit'] ?? 50;
    
    $result = supabaseRequest("control_logs?order=executed_at.desc&limit={$limit}");
    
    if ($result !== false) {
        sendJsonResponse(true, $result);
    } else {
        sendJsonResponse(false, null, 'Failed to fetch logs');
    }
}

// ==============================================
// ROUTE: POST /control (Điều khiển từ web)
// ==============================================
elseif ($path === 'control' && $requestMethod === 'POST') {
    $input = json_decode(file_get_contents('php://input'), true);
    $command = strtoupper($input['command'] ?? '');
    
    $validCommands = ['PUMP_ON', 'PUMP_OFF', 'AUTO_ON', 'AUTO_OFF', 'RELOAD_CONFIG'];
    
    if (!in_array($command, $validCommands)) {
        sendJsonResponse(false, null, 'Invalid command');
    }
    
    // Lưu lệnh vào database để ESP32 đọc
    $logData = [
        'command' => $command,
        'source' => 'web',
        'user_id' => null
    ];
    
    supabaseRequest('control_logs', 'POST', $logData);
    
    // Lưu lệnh pending
    $commandData = [
        'command' => $command,
        'executed' => false
    ];
    
    $result = supabaseRequest('pending_commands', 'POST', $commandData);
    
    if ($result) {
        sendJsonResponse(true, [
            'command' => $command,
            'message' => 'Command queued successfully'
        ]);
    } else {
        sendJsonResponse(false, null, 'Failed to queue command');
    }
}

// ==============================================
// ROUTE: GET /pending-commands (ESP32 lấy lệnh)
// ==============================================
elseif ($path === 'pending-commands' && $requestMethod === 'GET') {
    $result = supabaseRequest('pending_commands?executed=eq.false&order=created_at.asc&limit=10');
    
    if ($result !== false) {
        sendJsonResponse(true, $result);
    } else {
        sendJsonResponse(false, null, 'Failed to fetch commands');
    }
}

// ==============================================
// ROUTE: POST /mark-executed (ESP32 đánh dấu đã thực thi)
// ==============================================
elseif ($path === 'mark-executed' && $requestMethod === 'POST') {
    $input = json_decode(file_get_contents('php://input'), true);
    $commandId = $input['id'] ?? null;
    
    if (!$commandId) {
        sendJsonResponse(false, null, 'Missing command ID');
    }
    
    $result = supabaseRequest(
        "pending_commands?id=eq.{$commandId}",
        'PUT',
        ['executed' => true]
    );
    
    sendJsonResponse(true, ['updated' => true]);
}

// ==============================================
// ROUTE: GET /config (Lấy cấu hình)
// ==============================================
elseif ($path === 'config' && $requestMethod === 'GET') {
    $result = supabaseRequest('system_config?id=eq.1&limit=1');
    
    if ($result && count($result) > 0) {
        sendJsonResponse(true, $result[0]);
    } else {
        sendJsonResponse(false, null, 'Config not found');
    }
}

// ==============================================
// ROUTE: PUT /config (Cập nhật cấu hình)
// ==============================================
elseif ($path === 'config' && $requestMethod === 'PUT') {
    $input = json_decode(file_get_contents('php://input'), true);
    
    $result = supabaseRequest('system_config?id=eq.1', 'PUT', $input);
    
    if ($result && count($result) > 0) {
        sendJsonResponse(true, $result[0]);
    } else {
        sendJsonResponse(false, null, 'Failed to update config');
    }
}

// ==============================================
// ROUTE: GET /stats/daily (Thống kê theo ngày)
// ==============================================
elseif ($path === 'stats/daily' && $requestMethod === 'GET') {
    $days = $_GET['days'] ?? 7;
    $dateThreshold = date('Y-m-d', strtotime("-{$days} days"));
    
    $result = supabaseRequest(
        "daily_stats?date=gte.{$dateThreshold}&order=date.desc"
    );
    
    if ($result !== false) {
        sendJsonResponse(true, $result);
    } else {
        sendJsonResponse(false, null, 'Failed to fetch stats');
    }
}

// ==============================================
// DEFAULT: 404
// ==============================================
else {
    http_response_code(404);
    sendJsonResponse(false, null, 'Endpoint not found: ' . $path);
}
?>