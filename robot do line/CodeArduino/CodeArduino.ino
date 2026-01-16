// --- Cấu hình chân điều khiển động cơ ---
const int in1 = 8;
const int in2 = 9;
const int in3 = 10;
const int in4 = 11;
const int enA = 5;
const int enB = 6;

// --- Cấu hình chân cảm biến (từ trái sang phải) ---
const int T1 = 2;   // Trái ngoài
const int T2 = 12;  // Trái giữa
const int S_S = 3;  // Giữa
const int P1 = 4;   // Phải giữa
const int P2 = 13;  // Phải ngoài

// --- Biến tốc độ ---
int speedPWM = 70;   // Tốc độ động cơ (0–255)

void setup() {
  Serial.begin(9600);

  // Đặt chân động cơ là OUTPUT
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  pinMode(enA, OUTPUT);
  pinMode(enB, OUTPUT);

  analogWrite(enA, speedPWM);
  analogWrite(enB, speedPWM);

  // Đặt chân cảm biến là INPUT
  pinMode(T1, INPUT);
  pinMode(T2, INPUT);
  pinMode(S_S, INPUT);
  pinMode(P1, INPUT);
  pinMode(P2, INPUT);

  idle();  // Dừng ban đầu
  Serial.println("=== Line Following Robot Started ===");
  Serial.println("Format: T1 T2 S P1 P2");
}

// --- Các hàm điều khiển ---
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

void turnLeft() {  // Rẽ trái
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
}

void turnRight() { // Rẽ phải
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

// --- Vòng lặp chính ---
void loop() {
  // Đọc cảm biến 
  int t1 = digitalRead(T1);
  int t2 = digitalRead(T2);
  int ss = digitalRead(S_S);
  int p1 = digitalRead(P1);
  int p2 = digitalRead(P2);

  // In trạng thái cảm biến ra Serial
  Serial.print(t1); Serial.print(" ");
  Serial.print(t2); Serial.print(" ");
  Serial.print(ss); Serial.print(" ");
  Serial.print(p1); Serial.print(" ");
  Serial.println(p2);

  // --- Logic dò line ---
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

  delay(50); // giảm tốc độ in Serial, tránh nhiễu
}
