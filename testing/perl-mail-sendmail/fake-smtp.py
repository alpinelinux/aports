import smtpd
import asyncore

class FakeSMTPServer(smtpd.SMTPServer):
    """A Fake smtp server"""

    def __init__(*args, **kwargs):
        smtpd.SMTPServer.__init__(*args, **kwargs)

    def process_message(*args, **kwargs):
        pass

if __name__ == "__main__":
    smtp_server = FakeSMTPServer(('127.0.0.1', 2525), None)
    try:
        asyncore.loop(timeout=1, count=30)
    except KeyboardInterrupt:
        smtp_server.close()
