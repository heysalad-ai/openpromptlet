import os,termios,tty,select
class RawUSB:
    def __init__(self, port):
        self.fd=os.open(port,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK);self.buffer=b''
        tty.setraw(self.fd);a=termios.tcgetattr(self.fd);a[2]&=~termios.HUPCL;a[4]=a[5]=termios.B115200;termios.tcsetattr(self.fd,termios.TCSANOW,a)
    def write(self,data):
        while data:
            if not select.select([],[self.fd],[],1)[1]:raise RuntimeError('USB write timeout')
            n=os.write(self.fd,data);data=data[n:]
    def readline(self):
        if b'\n' not in self.buffer and select.select([self.fd],[],[],.2)[0]:
            self.buffer+=os.read(self.fd,16384)
        if b'\n' in self.buffer:line,self.buffer=self.buffer.split(b'\n',1);return line+b'\n'
        if len(self.buffer)>16384:self.buffer=b''
        return b''
    def close(self):
        if self.fd is not None:os.close(self.fd);self.fd=None
