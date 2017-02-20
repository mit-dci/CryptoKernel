Json = (loadfile("./json.lua"))()
local json = Json.new()
thisTransaction = json:decode(txJson)
thisInput = json:decode(thisInputJson)

sandbox_env = {Crypto = {new = Crypto.new, getPublicKey = Crypto.getPublicKey, getPrivateKey = Crypto.getPrivateKey,
                        setPublicKey = Crypto.setPublicKey, setPrivateKey = Crypto.setPrivateKey,
                        getStatus = Crypto.getStatus, sign = Crypto.sign, verify = Crypto.verify,},
               Json = {new = Json.new, decode = Json.decode,},
               thisTransaction = thisTransaction,
               thisInput = thisInput,
               txJson = txJson,
               outputSetId = outputSetId,
               Blockchain = {getBlock = Blockchain.getBlock,},
              }

local function setfenv(fn, env)
  local i = 1
  while true do
    local name = debug.getupvalue(fn, i)
    if name == "_ENV" then
      debug.upvaluejoin(fn, i, (function()
        return env
      end), 1)
      break
    elseif not name then
      break
    end

    i = i + 1
  end

  return fn
end

pc = 0

function programCounterHook()
    pc = pc + 50
    if pc > pcLimit then
        error("Instruction limit reached")
    end
end

function run_sandbox(sb_env, sb_func, ...)
  if (not sb_func) then return nil end
  setfenv(sb_func, sb_env)
  debug.sethook(programCounterHook, "", 50)
  local sb_ret={_ENV.pcall(sb_func, ...)}
  return _ENV.table.unpack(sb_ret)
end

function verifyTransaction(bytecode)
    local status, lz4 = pcall(require, "lz4")
    if(status) then
        f = load(lz4.decompress(bytecode))
        pcall_rc, result_or_err_msg = run_sandbox(sandbox_env, f)
        if type(result_or_err_msg) ~= "boolean" then
            print(result_or_err_msg)
            return false
        else
            return result_or_err_msg
        end
    else
        print("Failed to load lz4")
        return false
    end
end

