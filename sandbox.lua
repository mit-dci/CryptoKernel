Json = (loadfile("./json.lua"))()
local json = Json.new()
thisTransaction = json:decode(txJson)
thisInput = json:decode(thisInputJson)

function jsonStrToObj(str)
    if str == "" then return nil end
    return json:decode(str)
end

function getBlock(id)
    local res = Blockchain.getBlock(id)
    return jsonStrToObj(res)
end

function getTransaction(id)
    local res = Blockchain.getTransaction(id)
    return jsonStrToObj(res)
end

function getOutput(id)
    local res = Blockchain.getOutput(id)
    return jsonStrToObj(res)
end

function getInput(id)
    local res = Blockchain.getInput(id)
    return jsonStrToObj(res)
end

sandbox_env = {Crypto = {new = Crypto.new, getPublicKey = Crypto.getPublicKey, getPrivateKey = Crypto.getPrivateKey,
                        setPublicKey = Crypto.setPublicKey, setPrivateKey = Crypto.setPrivateKey,
                        getStatus = Crypto.getStatus, sign = Crypto.sign, verify = Crypto.verify,},
               Json = {new = Json.new, decode = Json.decode,},
               sha256 = sha256,
               thisTransaction = thisTransaction,
               thisInput = thisInput,
               outputSetId = outputSetId,
               Blockchain = {getBlock = getBlock, getTransaction = getTransaction,
                             getOutput = getOutput, getInput = getInput,},
               assert = assert,
               error = error,
               ipairs = ipairs,
               next = next,
               pairs = pairs,
               pcall = pcall,
               select = select,
               tonumber = tonumber,
               tostring = tostring,
               type = type,
               xpcall = xpcall,
               string = {len = string.len,
                         pack = string.pack,
                         packsize = string.packsize,
                         reverse = string.reverse,
                         sub = string.sub,
                         unpack = string.unpack,},
               utf8 = {char = utf8.char,
                       charpattern = utf8.charpattern,
                       codes = utf8.codes,
                       codepoint = utf8.codepoint,
                       len = utf8.len,
                       offset = utf8.offset,},
               table = {concat = table.concat,
                        insert = table.insert,
                        move = table.move,
                        pack = table.pack,
                        remove = table.remove,
                        sort = table.sort,
                        unpack = table.unpack,},
               math = {abs = math.abs,
                       acos = math.acos,
                       asin = math.asin,
                       atan = math.atan,
                       ceil = math.ceil,
                       cos = math.cos,
                       deg = math.deg,
                       exp = math.exp,
                       floor = math.floor,
                       fmod = math.fmod,
                       huge = math.huge,
                       log = math.log,
                       max = math.max,
                       maxinteger = math.maxinteger,
                       min = math.min,
                       mininteger = math.mininteger,
                       modf = math.modf,
                       pi = math.pi,
                       rad = math.rad,
                       sin = math.sin,
                       sqrt = math.sqrt,
                       tan = math.tan,
                       tointeger = math.tointeger,
                       type = math.type,
                       ult = math.ult,},
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
  if (not sb_func) then return "Not a valid function" end
  setfenv(sb_func, sb_env)
  collectgarbage("stop")
  debug.sethook(programCounterHook, "", 50)
  local sb_ret={_ENV.pcall(sb_func, ...)}
  debug.sethook()
  collectgarbage("restart")
  return _ENV.table.unpack(sb_ret)
end

function verifyTransaction(bytecode)
    local status, lz4 = pcall(require, "lz4")
    if(status) then
        f = load(lz4.decompress(bytecode))
        pcall_rc, result_or_err_msg = run_sandbox(sandbox_env, f)
        if type(result_or_err_msg) ~= "boolean" then
            print(result_or_err_msg)
            return false, ""
        else
            return result_or_err_msg, ""
        end
    else
        return false, "Failed to load lz4"
    end
end

