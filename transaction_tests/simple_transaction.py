import requests
import json
import base64
import time

def request(method, params):
    url = "http://localhost:8000" #TODO change to correct might be 8383
    encoded_str = str(base64.b64encode(b'[username]:[password]'), 'utf-8')
    headers = {
        "content-type": "application/json",
        "Authorization": "Basic " + encoded_str}
    payload = {
        "method": method,
        "params": params,
        "jsonrpc": "2.0",
        "id": 0,
    }
    response = requests.post(url, data=json.dumps(payload), headers=headers).json()  
    return response

inp = request("listunspentoutputs", {"account": "python"})["result"]["outputs"].pop()

tx = {"inputs":     [{"outputId": inp["id"], "data": {}}], 
      "outputs":    [{"value": [amount], 
                      "nonce": [number], 
                      "data": {"publicKey": "[public key]"}}], 
      "timestamp":  time.time()
      }                

signed = request("signtransaction", {"transaction": tx, "password": "mypassword"})["result"]

print(request("sendrawtransaction", {"transaction": signed}))