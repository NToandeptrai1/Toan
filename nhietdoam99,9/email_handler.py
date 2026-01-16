import smtplib
import logging
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.image import MIMEImage
from datetime import datetime, timezone, timedelta
from email.utils import formataddr
import os
import smtplib
VN_TZ = timezone(timedelta(hours=7))
logger = logging.getLogger('XParking.Email')

class EmailHandler:
    def __init__(self, system_config):
        self.config = system_config
        
    def get_vn_time(self, format_str='%d/%m/%Y %H:%M:%S'):
        return datetime.now(VN_TZ).strftime(format_str)

    def send_email(self, subject, body, image_name=None, to_email=None, reply_to=None):
        try:
            if not to_email:
                to_email = self.config.config['email_recipient']
                
            if not to_email:
                logger.warning("Không có email người nhận")
                return False

            sender_email = self.config.config['email_sender']
            sender_password = self.config.config['email_password']
            
            if not sender_email or not sender_password:
                logger.warning("Thiếu thông tin email gửi")
                return False
            
            msg = MIMEMultipart()
            # 👇 Đây là chỗ set tên hiển thị cho người gửi
            msg['From'] = formataddr(("Thông Báo từ Agent XParking", sender_email))
            msg['To'] = to_email
            msg['Subject'] = subject
            msg.attach(MIMEText(body, 'html', 'utf-8'))

            if reply_to:
                msg['Reply-To'] = reply_to

            # Đính kèm ảnh nếu có
            if image_name:
                self.attach_image(msg, image_name)

            with smtplib.SMTP('smtp.gmail.com', 587) as server:
                server.starttls()
                server.login(sender_email, sender_password)
                server.send_message(msg)
            
            logger.info(f"✅ Đã gửi email: {subject}")
            return True
            
        except Exception as e:
            logger.error(f"❌ Lỗi gửi email: {e}")
            return False


    def attach_image(self, msg, image_name):
        img_folder = "img"  # Đường dẫn đến thư mục chứa hình ảnh
        img_path = os.path.join(img_folder, image_name)
        try:
            with open(img_path, 'rb') as img_file:
                img = MIMEImage(img_file.read())
                img.add_header('Content-ID', f'<{image_name}>')
                msg.attach(img)
        except Exception as e:
            logger.error(f"Lỗi đính kèm hình ảnh: {e}")

    def send_alert_email(self, gas_level, location):
        try:
            current_time = self.get_vn_time()
            
            subject = "CẢNH BÁO KHẨN CẤP - XPARKING"
            body = f"""
            <html>
                <body style="margin: 0; padding: 0; font-family: Arial, sans-serif; background-color: white;">
                    <div style="max-width: 600px; margin: 40px auto; background-color: white; border-radius: 10px; overflow: hidden; box-shadow: 0 4px 6px rgba(0,0,0,0.1);">
                        
                        <!-- Header -->
                        <div style="background: linear-gradient(135deg, #ff4444 0%, #cc0000 100%); color: white; padding: 40px 20px; text-align: center;">
                            <h1 style="margin: 0; font-size: 28px; font-weight: bold;">🚨 CẢNH BÁO KHẨN CẤP 🚨</h1>
                            <p style="margin: 10px 0 0 0; font-size: 16px; opacity: 0.95;">Phát hiện khói/gas tại bãi đỗ xe</p>
                        </div>
                        
                        <!-- Content -->
                        <div style="padding: 40px 30px; text-align: center;">
                            <h2 style="color: #333; font-size: 20px; margin: 0 0 30px 0;">TÌNH TRẠNG HỆ THỐNG</h2>
                            
                            <table style="width: 100%; max-width: 400px; margin: 0 auto; border-collapse: collapse;">
                                <tr>
                                    <td style="padding: 15px; text-align: left; border-bottom: 1px solid #eee;">
                                        <strong style="color: #666;">Thời gian:</strong>
                                    </td>
                                    <td style="padding: 15px; text-align: right; border-bottom: 1px solid #eee;">
                                        <span style="color: #333;">{current_time}</span>
                                    </td>
                                </tr>
                                <tr>
                                    <td style="padding: 15px; text-align: left; border-bottom: 1px solid #eee;">
                                        <strong style="color: #666;">Vị trí:</strong>
                                    </td>
                                    <td style="padding: 15px; text-align: right; border-bottom: 1px solid #eee;">
                                        <span style="color: #333;">{location}</span>
                                    </td>
                                </tr>
                                <tr>
                                    <td style="padding: 15px; text-align: left; border-bottom: 1px solid #eee;">
                                        <strong style="color: #666;">Mức độ gas:</strong>
                                    </td>
                                    <td style="padding: 15px; text-align: right; border-bottom: 1px solid #eee;">
                                        <span style="color: #ff4444; font-weight: bold;">{gas_level}</span>
                                    </td>
                                </tr>
                                <tr>
                                    <td style="padding: 15px; text-align: left;">
                                        <strong style="color: #666;">Trạng thái:</strong>
                                    </td>
                                    <td style="padding: 15px; text-align: right;">
                                        <span style="color: #ff4444; font-weight: bold;">Khẩn cấp</span>
                                    </td>
                                </tr>
                            </table>
                            
                            <!-- Warning Box -->
                            <div style="background-color: #fff3f3; border-left: 4px solid #ff4444; padding: 20px; margin: 30px 0; text-align: left;">
                                <p style="margin: 0; color: #cc0000; font-weight: bold; line-height: 1.6;">
                                    ⚠️ Vui lòng kiểm tra ngay khu vực bãi đỗ xe ngay lập tức
                                </p>
                            </div>
                            
                            <!-- Image -->
                            <div style="margin: 30px 0;">
                                <img src='cid:fire.gif' style="max-width: 300px; height: auto;" alt="Fire Alert">
                            </div>
                        </div>
                        <hr>
                        
                        <!-- Footer -->
                        <div style="background-color: #f9f9f9; padding: 20px; border-top: 1px solid #eee;">
                            <p style="margin: 0; font-size: 12px; color: #999;">
                                Email tự động này được gửi đi bởi Boo AI Agent, AI Agent của X Parking.
                                <br>
                                Thời gian gửi: {current_time}
                            </p>
                        </div>
                        
                    </div>
                </body>
            </html>
            """
            return self.send_email(subject, body, image_name='fire.gif')
        except Exception as e:
            logger.error(f"Lỗi gửi email cảnh báo: {e}")
            return False

    def send_recovery_email(self):
        """Gửi email thông báo hết khẩn cấp"""
        try:
            current_time = self.get_vn_time()
            
            subject = "HẾT KHẨN CẤP - XPARKING"
            body = f"""
            <html>
                <body style="margin: 0; padding: 0; font-family: Arial, sans-serif; background-color: white;">
                    <div style="max-width: 600px; margin: 40px auto; background-color: white; border-radius: 10px; overflow: hidden; box-shadow: 0 4px 6px rgba(0,0,0,0.1);">
                        
                        <!-- Header -->
                        <div style="background: linear-gradient(135deg, #00b894 0%, #00916e 100%); color: white; padding: 40px 20px; text-align: center;">
                            <h1 style="margin: 0; font-size: 28px; font-weight: bold;">KẾT THÚC TÌNH TRẠNG KHẨN CẤP</h1>
                            <p style="margin: 10px 0 0 0; font-size: 16px; opacity: 0.95;">Hệ thống đã trở lại bình thường</p>
                        </div>
                        
                        <!-- Content -->
                        <div style="padding: 40px 30px; text-align: center;">
                            <h2 style="color: #333; font-size: 20px; margin: 0 0 30px 0;">TÌNH TRẠNG HỆ THỐNG</h2>
                            
                            <table style="width: 100%; max-width: 400px; margin: 0 auto; border-collapse: collapse;">
                                <tr>
                                    <td style="padding: 15px; text-align: left; border-bottom: 1px solid #eee;">
                                        <strong style="color: #666;">Thời gian phục hồi:</strong>
                                    </td>
                                    <td style="padding: 15px; text-align: right; border-bottom: 1px solid #eee;">
                                        <span style="color: #333;">{current_time}</span>
                                    </td>
                                </tr>
                                <tr>
                                    <td style="padding: 15px; text-align: left; border-bottom: 1px solid #eee;">
                                        <strong style="color: #666;">Trạng thái gas:</strong>
                                    </td>
                                    <td style="padding: 15px; text-align: right; border-bottom: 1px solid #eee;">
                                        <span style="color: #00b894; font-weight: bold;">An toàn</span>
                                    </td>
                                </tr>
                                <tr>
                                    <td style="padding: 15px; text-align: left;">
                                        <strong style="color: #666;">Trạng thái hệ thống:</strong>
                                    </td>
                                    <td style="padding: 15px; text-align: right;">
                                        <span style="color: #00b894; font-weight: bold;">Bình thường</span>
                                    </td>
                                </tr>
                            </table>
                            
                            <!-- Success Box -->
                            <div style="background-color: #f0fdf4; border-left: 4px solid #00b894; padding: 20px; margin: 30px 0; text-align: left;">
                                <p style="margin: 0; color: #00916e; font-weight: bold; line-height: 1.6;">
                                    ✅ Bãi đỗ xe đã an toàn và sẵn sàng hoạt động trở lại!
                                </p>
                            </div>
                            
                            <!-- Image -->
                            <div style="margin: 30px 0;">
                                <img src='cid:dui.gif' style="max-width: 300px; height: auto;" alt="All Clear">
                            </div>
                        </div>
                        <hr>
                        
                        <!-- Footer -->
                        <div style="background-color: #f9f9f9; padding: 20px; border-top: 1px solid #eee;">
                            <p style="margin: 0; font-size: 12px; color: #999;">
                                Email tự động này được gửi đi bởi Boo AI Agent, AI Agent của X Parking.
                                <br>
                                Thời gian gửi: {current_time}
                            </p>
                        </div>
                        
                    </div>
                </body>
            </html>
            """
            return self.send_email(subject, body, image_name='dui.gif')
        except Exception as e:
            logger.error(f"Lỗi gửi email phục hồi: {e}")
            return False