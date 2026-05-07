import os
import sys
import base64
import json
import hashlib
import datetime
import random
import string
from cryptography.hazmat.primitives import serialization

PUB_KEY = """
MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDWnEqax70EE4go8U93srsjGnSgo
6xFqt2C8sFOhWxXZOmHs2y/SxPMZuludmzQR8KulnGg4TIgYy6pF0r/H9qct1ZRfm
jx8zCw4+zLlNpsTqcPuiAYYi7h1Cmpu7xQjxh+OeC0tZaEChZQli+1wgktirhvScZ
RDN6h8s4R0oteWQIDAQAB
"""

PRIVATE_KEY = """-----BEGIN PRIVATE KEY-----
MIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBANacSprHvQQTiCjx
T3eyuyMadKCjrEWq3YLywU6FbFdk6YezbL9LE8xm6W52bNBHwq6WcaDhMiBjLqkX
Sv8f2py3VlF+aPHzMLDj7MuU2mxOpw+6IBhiLuHUKam7vFCPGH454LS1loQKFlCW
L7XCCS2KuG9JxlEM3qHyzhHSi15ZAgMBAAECgYATCe58aLfWAr2TlETOg6aiaJhs
H9kKnSvlkA+iHagM4MDu7vX4ynpJKeAPkqX4nEUjI+mUsiW2RdY/3fcjRvonv21R
uGb9A5gotZnwv53BkZDP/TWMZ0LSsl1OGdxtCODYXfaMdj1MNSyKoyr750XiEPQN
Z2H7OncR4DsF3rU5YQJBAOtsFTyomrtKMR6NUuxmXDu0cygzic9qK4yFgHwfHF7Z
RyJqossEopmeujK6Hbtp1UFBI/Inc85hIvZpFJqrhAUCQQDpXoQ3gTmaesjJOBd0
pmIAs6p7qhYRLF1InCB6l+FwidaJ5zZth9JcF24+k9cn63Xtb0PLL8odXYOYlEZS
mPVFAkAoIOVB0K+HSy8yOP6wgwYnuyuB578O1tcTfdIX1im81SZ17F1RY7nfm5m7
edQFlRWfqN3asfTgcdhGzkSP1LqNAkEAoEAKFL9FGgJUnHBLEwwp4gd9+ztZueM4
D2M+nlBrO0c7rii6ZE5PMnPYfVox9bSnnyq3Z/BiHvYXJpAzFgb47QJAbTer0+Lk
89TxNaFvDxmJe1aTEnpMNhJp0OTc3BWsZg1EQjknxMnRr4/zDpdSltvmPz0PyQZt
N/fhBSZK5JSicQ==
-----END PRIVATE KEY-----"""

SEED = 1314
ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

def b58decode(v):
    n = 0
    for char in v:
        n = n * 58 + ALPHABET.index(char)

    pad = 0
    for char in v:
        if char == "1": pad += 1
        else: break
    return b"\x00" * pad + n.to_bytes((n.bit_length() + 7) // 8, 'big')

def rsa_pri_encode_raw(text_info, pri_key_pem):
    pri_key = serialization.load_pem_private_key(pri_key_pem.encode(), password=None)
    numbers = pri_key.private_numbers()
    d, n = numbers.d, numbers.public_numbers.n

    input_bytes = text_info.encode('utf-8')
    block_size = 128
    chunk_size = 128 
    final_res = bytearray()
    for i in range(0, len(input_bytes), chunk_size):
        chunk = input_bytes[i:i+chunk_size]
        padded = chunk.ljust(block_size, b'\x00')
        m = int.from_bytes(padded, byteorder='big')
        c = pow(m, d, n)
        final_res.extend(c.to_bytes(block_size, byteorder='big'))

    return base64.b64encode(final_res).decode('utf-8')


def shift_encrypt(input_bytes, seed):
    def to_int16(n):
        n = n & 0xFFFF
        return n if n <= 0x7FFF else n - 0x10000

    v7 = to_int16(seed)
    output_chars = []
    for byte_val in input_bytes:
        hibyte = (v7 >> 8) & 0xFF
        cipher_byte = byte_val ^ hibyte
        char1 = chr((cipher_byte // 26) + 65)
        char2 = chr((cipher_byte % 26) + 65)
        output_chars.append(char1 + char2)
        v7 = to_int16(-12691 * (cipher_byte + v7) + 22719)
    return "".join(output_chars)

def generate_random_string(length=16):
    characters = string.ascii_uppercase + string.digits
    return ''.join(random.choice(characters) for _ in range(length))

def create_registration_file(app_id, app_key, fingerprint, start_time, end_time, platform):
    active_key = generate_random_string(19) #要求长度16或者19
    important_dict = {
        "activeKey": active_key,
        "deviceFingerPrint": fingerprint,
        "sdkType":"ArcFace",
        "startTime": start_time,
        "endTime": end_time,
        "platform": platform
    }

    important_json = json.dumps(important_dict, separators=(',', ':'))
    #print(important_json)
    important_b64 = rsa_pri_encode_raw(important_json, PRIVATE_KEY)
    #print(important_b64)
    base_info = {
        "appId": app_id,
        "appKey": app_key,
        "sdkVersion": "2.2"
    }

    main_structure = {
        "assistantVersion": "1.0",
        "baseInfo": json.dumps(base_info, separators=(',', ':')),
        "fileVersion": "2.0",
        "importantInfo": important_b64
    }

    final_json_str = json.dumps(main_structure, separators=(',', ':'))
    print(final_json_str)
    content = shift_encrypt(final_json_str.encode('utf-8'), SEED)
    return content

def shift_decrypt(input_str, seed):
    def to_int16(n):
        n = n & 0xFFFF
        return n if n <= 0x7FFF else n - 0x10000

    v7 = to_int16(seed)
    clean_input = "".join(input_str.split()).replace('"', '')
    reconstructed_bytes = []
    for i in range(len(clean_input) // 2):
        v4 = (26 * (ord(clean_input[2 * i]) - 65) + (ord(clean_input[2 * i + 1]) - 65)) & 0xFF
        reconstructed_bytes.append(v4)

    result = []
    for byte_val in reconstructed_bytes:
        hibyte = (v7 >> 8) & 0xFF
        result.append(byte_val ^ hibyte)
        v7 = to_int16(-12691 * (byte_val + v7) + 22719)
    return bytes(result)

def rsa_pub_decode(b64_input, pub_key_bytes):
    try:
        public_key = serialization.load_der_public_key(pub_key_bytes)
    except:
        public_key = serialization.load_pem_public_key(pub_key_bytes)

    numbers = public_key.public_numbers()
    n, e = numbers.n, numbers.e
    encrypted_data = base64.b64decode(b64_input)
    decrypted_final = bytearray()
    block_size = 128 
    for i in range(0, len(encrypted_data), block_size):
        block = encrypted_data[i:i+block_size]
        if len(block) < block_size: break
        c = int.from_bytes(block, byteorder='big')
        m = pow(c, e, n)
        m_bytes = m.to_bytes(block_size, byteorder='big')
        decrypted_final.extend(m_bytes)
    return bytes(decrypted_final)

def verify_sdk_pair(data_dict):
    base_info_raw = data_dict.get("baseInfo", "{}")
    if isinstance(base_info_raw, str):
        base_info = json.loads(base_info_raw)
    else:
        base_info = base_info_raw

    app_id = base_info.get("appId", "")
    app_key = base_info.get("appKey", "")
    if not app_id or not app_key:
        print("err: not app_id or not app_key")
        return

    v8 = b58decode(app_id)
    v12 = b58decode(app_key)
    v7 = hashlib.md5(v8).digest()
    if v7 == v12[0:16]:
        print("app_key match success")
    else:
        print("app_key match fail")
    #print(f"AppId MD5: {v7.hex()}")
    print(f"AppKey Prefix: {v12[0:16].hex()}")

def shift_encrypt_pubkey(raw_bytes, seed):
    def to_int16(n):
        n = n & 0xFFFF
        return n if n <= 0x7FFF else n - 0x10000

    v7 = to_int16(seed)
    output_chars = []
    for byte_val in raw_bytes:
        hibyte = (v7 >> 8) & 0xFF
        cipher_byte = byte_val ^ hibyte

        char1 = chr((cipher_byte // 26) + 65)
        char2 = chr((cipher_byte % 26) + 65)
        output_chars.append(char1 + char2)

        v7 = to_int16(-12691 * (cipher_byte + v7) + 22719)
        
    return "".join(output_chars)

def rsa_decrypt_raw_with_serialization(base64_str, key):
    if isinstance(key, str):
        key = key.encode()

    key = serialization.load_pem_private_key(key, password=None)
    pn = key.private_numbers()
    n = pn.public_numbers.n
    d = pn.d

    encrypted_data = base64.b64decode(base64_str)
    decrypted_payload = b""
    for i in range(0, len(encrypted_data), 128):
        chunk = encrypted_data[i:i+128]
        if len(chunk) < 128:
            break
        c = int.from_bytes(chunk, byteorder='big')
        m = pow(c, d, n)
        decrypted_block = m.to_bytes(128, byteorder='big')
        decrypted_payload += decrypted_block[1:]

    return decrypted_payload.rstrip(b'\x00').decode('utf-8', errors='ignore')

def ultimate_json_parse(raw_str):
    try:
        return json.loads(raw_str)
    except json.JSONDecodeError:
        pass

    indices = [i for i, char in enumerate(raw_str) if char == '}']
    for index in reversed(indices):
        candidate = raw_str[:index + 1]
        try:
            return json.loads(candidate)
        except json.JSONDecodeError:
            continue
    return None

def main():
    input_file = sys.argv[1]
    if not os.path.exists(input_file):
        print(f"input_file not found: '{input_file}'")
        return
    with open(input_file, 'r', encoding='utf-8') as f:
        content = f.read().strip()
    decrypted_result = rsa_decrypt_raw_with_serialization(content, PRIVATE_KEY)
    data = ultimate_json_parse(decrypted_result)
    platform = data.get("platform")
    if platform == "windows_x64":
        app_id = "CHXzt9XtJEaL5Fkp3wxMfH6Nzasux9knPAn4rAqhCmQK"
        app_key = "7o725e4mR6g7xcfhsvSztPnt8jpWq8t6FbgnuhnSMzgv"
    else:
        app_id = "5dgTYg9STpEoEvJDcTUJKG11ePeCrgQ75kJpspSqsChg"
        app_key = "Gu8z244xvDtXzPSDhrHbHiqTJuTmwRZHkLBYdhHMzeak"
    deviceFingerPrint = data.get("deviceFingerPrint")
    
    start  = str(int(datetime.datetime.strptime("2026-04-29 16:17:55", "%Y-%m-%d %H:%M:%S").timestamp()))
    end    = str(int(datetime.datetime.strptime("2036-04-29 16:17:55", "%Y-%m-%d %H:%M:%S").timestamp()))
    try:
        final_content = create_registration_file(app_id, app_key, deviceFingerPrint, start, end, platform)
        pub_der_bytes = base64.b64decode(PUB_KEY)
        cipher_pubkey = shift_encrypt_pubkey(pub_der_bytes, SEED)
        try:
            content = final_content
            v79_decrypted = shift_decrypt(content, SEED)
            json_str = v79_decrypted.decode('utf-8', errors='ignore').strip('\x00')
            #print(json_str)
            data_dict = json.loads(json_str)
            target_b64 = data_dict.get("importantInfo", "")
            if target_b64:
                rsa_key_data = shift_decrypt(cipher_pubkey, SEED)
                final_bytes = rsa_pub_decode(target_b64, rsa_key_data)
                result_text = final_bytes.strip(b'\x00').decode('utf-8', errors='ignore')
                print("importantInfo:"+ result_text)
                print("--- cipher_pubkey ---")
                print(cipher_pubkey)
            else:
                print("[-] JSON not found importantInfo")
            verify_sdk_pair(data_dict)
        except Exception as e:
            print(f"[-] fail: {e}")
        with open("license.txt", "w") as f:
            f.write(final_content)
    except Exception as e:
        print(f"err: {e}")

if __name__ == "__main__":
    main()
