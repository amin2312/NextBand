from req_base import CrosHandler, ParseHandler
from conf import Conf

import smtplib
from email.header import Header
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText

import schemas
import json


def new_emailer(sender: str = 'admin'):
    sender_conf = Conf.smtp.senders.get(sender)
    if not sender_conf:
        raise Exception(f"Sender '{sender}' not found in SMTP configuration.")

    server = smtplib.SMTP_SSL(Conf.smtp.host, Conf.smtp.port)
    server.login(sender_conf.username, sender_conf.password)
    return server, sender_conf


class SendAnEmail(ParseHandler, CrosHandler):
    async def post(self):
        is_html = False
        results = []
        server = None
        try:
            param = self.parse_body(schemas.SendAnEmail)
            sender = 'admin'
            addresses = [param.to_address]
            p_body = param.body

            server, sender_conf = new_emailer(sender)
            for address in addresses:
                msg = MIMEMultipart()
                msg['From'] = f"{Header(sender_conf.from_name, 'utf-8').encode()} <{sender_conf.from_address}>"
                msg['To'] = address
                msg['Subject'] = param.subject

                if is_html:
                    msg.attach(MIMEText(p_body, 'html', 'utf-8'))
                else:
                    msg.attach(MIMEText(p_body, 'plain', 'utf-8'))

                try:
                    server.sendmail(sender_conf.from_address, [address], msg.as_string())
                    results.append({"Code": "ok", "Address": address})
                except Exception as e:
                    results.append({"Code": str(e), "Address": address})
        except Exception as e:
            results = [{"Code": str(e), "Address": "*"}]
        finally:
            if server:
                server.quit()

        self.write(json.dumps({"SendStatusSet": results}, ensure_ascii=False))


ROUTES = [
    (r"/send-an-email", SendAnEmail),
]
