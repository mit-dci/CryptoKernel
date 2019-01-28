#include "crypto.h"
#include "base64.h"

#include "contract.h"

CryptoKernel::ContractRunner::ContractRunner(CryptoKernel::Blockchain* blockchain,
        const uint64_t memoryLimit, const uint64_t instructionLimit) {
    this->memoryLimit = memoryLimit;
    this->pcLimit = instructionLimit;
    this->blockchain = blockchain;

    ud.reset(new int);
    *ud.get() = 0;
    luaState = lua_newstate(&CryptoKernel::ContractRunner::allocWrapper, this);
    luaL_openlibs(luaState);
    state.reset(new sel::State(luaState));

    blockchainInterface = new BlockchainInterface(blockchain);
}

void* CryptoKernel::ContractRunner::allocWrapper(void* thisPointer, void* ptr,
        size_t osize, size_t nsize) {
    CryptoKernel::ContractRunner* contractRunner = (CryptoKernel::ContractRunner*)
            thisPointer;
    return contractRunner->l_alloc_restricted(contractRunner->ud.get(), ptr, osize, nsize);
}

CryptoKernel::ContractRunner::~ContractRunner() {
    delete blockchainInterface;
    lua_close(luaState);
}

void* CryptoKernel::ContractRunner::l_alloc_restricted(void* ud, void* ptr, size_t osize,
        size_t nsize) {
    /* set limit here */
    int* used = (int*)ud;

    if(ptr == NULL) {
        /*
         * <http://www.lua.org/manual/5.2/manual.html#lua_Alloc>:
         * When ptr is NULL, osize encodes the kind of object that Lua is
         * allocating.
         *
         * Since we don’t care about that, just mark it as 0.
         */
        osize = 0;
    }

    if (nsize == 0) {
        free(ptr);
        *used -= osize; /* substract old size from used memory */
        return NULL;
    } else {
        if (*used + (nsize - osize) > memoryLimit) {/* too much memory in use */
            throw CryptoKernel::Blockchain::InvalidElementException("Memory limit reached");
        }
        ptr = realloc(ptr, nsize);
        if (ptr) {/* reallocation successful? */
            *used += (nsize - osize);
        }
        return ptr;
    }
}

std::string CryptoKernel::ContractRunner::compile(const std::string contractScript) {
    sel::State compilerState(true);

    if(!compilerState.Load("./compiler.lua")) {
        throw std::runtime_error("Failed to load compiler script");
    }

    const std::string compressedBytecode = compilerState["compile"](contractScript);

    return base64_encode((unsigned char*)compressedBytecode.c_str(),
                         compressedBytecode.size());
}

void CryptoKernel::ContractRunner::setupEnvironment(Storage::Transaction* dbTx,
        const CryptoKernel::Blockchain::transaction& tx,
        const CryptoKernel::Blockchain::input& input) {
    const int lim = this->pcLimit;
    (*state.get())["pcLimit"] = lim;

    (*state.get())["Crypto"].SetClass<CryptoKernel::Crypto, bool>("getPublicKey",
            &CryptoKernel::Crypto::getPublicKey,
            "getPrivateKey", &CryptoKernel::Crypto::getPrivateKey,
            "setPublicKey", &CryptoKernel::Crypto::setPublicKey,
            "setPrivateKey", &CryptoKernel::Crypto::setPrivateKey,
            "sign", &CryptoKernel::Crypto::sign,
            "verify", &CryptoKernel::Crypto::verify,
            "getStatus", &CryptoKernel::Crypto::getStatus
                                                                 );
    (*state.get())["txJson"] = CryptoKernel::Storage::toString(tx.toJson());
    (*state.get())["sha256"] = &CryptoKernel::Crypto::sha256;
    (*state.get())["thisInputJson"] = CryptoKernel::Storage::toString(input.toJson());
    (*state.get())["outputSetId"] = tx.getOutputSetId().toString();
    blockchainInterface->setTransaction(dbTx);
    (*state.get())["Blockchain"].SetObj((*blockchainInterface), "getBlock",
                                        &BlockchainInterface::getBlock, "getTransaction", &BlockchainInterface::getTransaction,
                                        "getOutput", &BlockchainInterface::getOutput, "getInput", &BlockchainInterface::getInput);
}

bool CryptoKernel::ContractRunner::evaluateValid(Storage::Transaction* dbTx,
        const CryptoKernel::Blockchain::transaction& tx) {
    for(const CryptoKernel::Blockchain::input& inp : tx.getInputs()) {
        const CryptoKernel::Blockchain::output out = CryptoKernel::Blockchain::dbOutput(
                    blockchain->utxos->get(dbTx, inp.getOutputId().toString()));
        const Json::Value data = out.getData();
        if(!data["contract"].empty()) {
            if(!this->evaluateScriptValid(dbTx, tx, inp, data["contract"].asString())) {
                return false;
            }
        }
    }

    return true;
}

bool CryptoKernel::ContractRunner::evaluateScriptValid(Storage::Transaction* dbTx,
        const CryptoKernel::Blockchain::transaction& tx,
        const CryptoKernel::Blockchain::input& inp, 
        const std::string& script) {
            
    setupEnvironment(dbTx, tx, inp);
    if(!(*state.get()).Load("./sandbox.lua")) {
        throw std::runtime_error("Failed to load sandbox.lua");
    }

    bool result = false;
    std::string errorMessage = "";

    state->HandleExceptionsWith([&](int, std::string msg, std::exception_ptr) {
                                        errorMessage = msg; 
                                        result = false;
                                    });

    try {
        sel::tie(result, errorMessage) = (*state.get())["verifyTransaction"](base64_decode(
                                                script));
    } catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
        return false;
    }

    if(errorMessage != "") {
        throw std::runtime_error(errorMessage);
    }

    return result;
}
