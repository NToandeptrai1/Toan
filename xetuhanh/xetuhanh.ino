// --- Cấu hình chân điều khiển động cơ ---
const int in1 = 26;
const int in2 = 27;
const int in3 = 14;
const int in4 = 12;
const int enA = 25;
const int enB = 33;

// --- Cấu hình chân cảm biến (từ trái sang phải) ---
const int sensorT1 = 19;  // Trái ngoài
const int sensorT2 = 18;  // Trái giữa
const int sensorS  = 5;   // Giữa
const int sensorP1 = 17;  // Phải giữa
const int sensorP2 = 16;  // Phải ngoài

// --- Cấu hình PWM cho ESP32 ---
const int pwmFreq = 5000;    // Tần số PWM (Hz)
const int pwmResolution = 8; // Độ phân giải 8-bit (0-255)

// --- Biến tốc độ ---
int speedPWM = 150;  // Tốc độ động cơ giống Arduino (0–255)

void setup() {
  Serial.begin(115200);

  // --- Cấu hình PWM mới cho ESP32 Core 3.x ---
  ledcAttach(enA, pwmFreq, pwmResolution);
  ledcAttach(enB, pwmFreq, pwmResolution);

  // Đặt tốc độ ban đầu
  ledcWrite(enA, speedPWM);
  ledcWrite(enB, speedPWM);

  // Đặt chân động cơ là OUTPUT
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  // Đặt chân cảm biến là INPUT
  pinMode(sensorT1, INPUT);
  pinMode(sensorT2, INPUT);
  pinMode(sensorS, INPUT);
  pinMode(sensorP1, INPUT);
  pinMode(sensorP2, INPUT);

  idle(); // Dừng ban đầu
  Serial.println("=== Line Following Robot Started (ESP32) ===");
  Serial.println("Format: T1 T2 S P1 P2");
}

// --- Các hàm điều khiển (GIỐNG ARDUINO) ---
void idle() {  // Dừng
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
}

void turnUp() {  // Đi thẳng
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

void turnLeft() {  // Rẽ trái (ĐÃ SỬA theo Arduino)
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
}

void turnRight() { // Rẽ phải (ĐÃ SỬA theo Arduino)
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

// --- Vòng lặp chính (LOGIC GIỐNG ARDUINO) ---
void loop() {
  // Đọc cảm biến
  int t1 = digitalRead(sensorT1);
  int t2 = digitalRead(sensorT2);
  int ss = digitalRead(sensorS);
  int p1 = digitalRead(sensorP1);
  int p2 = digitalRead(sensorP2);

  // In trạng thái cảm biến ra Serial
  Serial.print(t1); Serial.print(" ");
  Serial.print(t2); Serial.print(" ");
  Serial.print(ss); Serial.print(" ");
  Serial.print(p1); Serial.print(" ");
  Serial.println(p2);

  // --- Logic dò line GIỐNG ARDUINO ---
  if ((t1==0 && t2==0 && ss==0 && p1==0 && p2==0) ||     // không thấy line
      (t1==1 && t2==1 && ss==1 && p1==1 && p2==1)) {     // toàn line
    idle();
  }
  else if (t1==1 || t2==1) {      // lệch trái
    turnLeft();
  }
  else if (p1==1 || p2==1) {      // lệch phải
    turnRight();
  }
  else if (ss==1) {               // line ở giữa
    turnUp();
  }
  else {                          // không rõ → dừng
    idle();
  }

  delay(50); // giảm tốc độ in Serial, tránh nhiễu (giống Arduino)
}