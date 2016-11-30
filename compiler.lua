function compile(contractScript)
    local s = string.dump(load(contractScript))
    local lz4 = require("lz4")
    local options = {compression_level = 16}
    return lz4.compress(s, options)
end
