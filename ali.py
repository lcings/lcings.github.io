import os
import re
import binascii
from datetime import datetime
from cryptography.hazmat.primitives import serialization

PUBLIC_KEY_PEM = """-----BEGIN PUBLIC KEY-----
MIIEIjANBgkqhkiG9w0BAQEFAAOCBA8AMIIECgKCBAEA43KlevmfJs1fupMHtUVsLBrMHH1U81Xr
HMYdG1YkcdYRTlbiZ3Ly4ZxjcMwTQPfsGRzwKyU3rzKmhdGrRNzFxWAcmC/XvFfxYP4nlVxqYhSQ
p/pUxZt30ccKGfbZA2V++89BWtYIvQuyRHTIwaRhQ+LsUJkaqyVwAqA/GkdtapFISfnP6gGNEwUU
3zdsdsEmutK8it/2hB54kmuS1rWSGDBTWIsvTRPyNYsRpf0pimyWwrxxFnXd5yxe/NpD3N3Fhh2X
9XADSUSl/w5vdGy3I1uiRBTJi9Cgj0vsRfOc8ml0uiHv6aZyj1rjJ0ACupnBbU95AFBAjBIvJWhJ
w6fpc8yx/hZj6swFSt/R7AXLSgc3OOijORO1zgVKc+xCDxgtMuvw9sDUz6c2HBRG+BmxWlR5Z/9Q
y7uO/hwZyyhHXRe3nVyTzObRyPYk/E2L+WTREVPDmasba9fAsLM5JAFaa16mY/NrfYUzYIFmN3u4
H/kALxS9+AaYwi0Qx3bNWgAScX6XwsULpfst7hQiWH7B5u1Z8xdRHLL6PlpmsJ8jO1s2O1xCh1X6
xqzcXzwjPZCIXr7L6ygXS6oqvZOjH6HY8hpaL9/lm0Saflo92Gr2omaMFYxo1M1ndqrtdQ7Ezsh/
yHqNA/oW6Dhvy+pjCx06PgkQcxDoTeEv+4uYZ5HA1oZBZB3VmcBSQXQ6cDBLK8VO4J0NqkWavCFW
g1aoY8R+YoGY3cTJh3KTk4adSL0raVG8l8cR3ibfINgo/Hk4vPDvC1mrxrbYwWkbWdsJZSKrWNwY
7QyyhdyPHSGbPy9hklb89HU9Sq82qQJp4y0/aPz+k13IB5O3b6ZFIx2zsiJIH+M87ysIi8XwKELA
AvI2dlkciKhWjBUbS2FseK3pb2bdIseHgK3A7Ifo6hzKXqFXhMu8jFvlccyMC0qcJC4yY6JMwtMA
XIp3VCVh51HYoLSGMVPIPmZ5ANy2sHz9+DwPQ8cer3kZscpcOlfSS9ZIX2XuRnAhzuT1hfU68zCd
haL6X2RwLBzJrNFFavAYsjjebhkeFvHJ4V5Ff9rMBuO91cDRp3fUFSEUK40nrCHnietPXCmDUyRs
tOd5RAQNT6N1Br72pm8+K8zaeH/9nNrCg68W6ujv5wArINJbcH8D5k72iewAdqgaeNqDIn9If02O
PUEU6MphwSES9qZmESP6ETb3P2H3XbIymax14xjWM5SdhGSC6RTuFKQJKud9oCPk0g/WFiCYY/jZ
DhB8HiDBvtY4qKLB7+t1it4GReBx9+V3w8roAxdvI6zy4tSiREuIzUpTeGxIqLW8yFzdOb4oflKT
G4lS2XMnES9OGW2SAEVARDHdtkFUSXN1bZFuY+oHTwIDAQAB
-----END PUBLIC KEY-----"""

FILE_PATH = "andriod.license" 
def decrypt_file_and_extract_time(file_path, pem_key):
    if not os.path.exists(file_path):
        print(f"[-] file not exists")
        return
    try:
        with open(file_path, 'rb') as f:
            file_data = f.read()
        try:
            clean_text = file_data.decode('utf-8', errors='ignore').strip()
            clean_hex = "".join([c for c in clean_text if c.isalnum()])
            ciphertext = binascii.unhexlify(clean_hex)
        except Exception:
            ciphertext = file_data
        public_key = serialization.load_pem_public_key(pem_key.encode())
        numbers = public_key.public_numbers()
        n, e = numbers.n, numbers.e
        c = int.from_bytes(ciphertext, byteorder='big')
        m = pow(c, e, n)
        key_size_bytes = (n.bit_length() + 7) // 8
        decrypted_bytes = m.to_bytes(key_size_bytes, byteorder='big').lstrip(b'\x00')
        plain_text = decrypted_bytes.decode('utf-8', errors='ignore')
        print(plain_text)
    except Exception as exc:
        print(f"err: {str(exc)}")
if __name__ == "__main__":
    decrypt_file_and_extract_time(FILE_PATH, PUBLIC_KEY_PEM)
