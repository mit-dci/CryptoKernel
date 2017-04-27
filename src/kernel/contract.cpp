#include "crypto.h"
#include "base64.h"

#include "contract.h"

CryptoKernel::ContractRunner::ContractRunner(CryptoKernel::Blockchain* blockchain, const uint64_t memoryLimit, const uint64_t instructionLimit)
{
    this->memoryLimit = memoryLimit;
    this->pcLimit = instructionLimit;
    this->blockchain = blockchain;

    ud.reset(new int);
    *ud.get() = 0;
    lua_State* _l = lua_newstate(&CryptoKernel::ContractRunner::allocWrapper, this);
    luaL_openlibs(_l);
    state.reset(new sel::State(_l));

    blockchainInterface = new BlockchainInterface(blockchain);
}

void* CryptoKernel::ContractRunner::allocWrapper(void* thisPointer, void* ptr, size_t osize, size_t nsize)
{
    CryptoKernel::ContractRunner* contractRunner = (CryptoKernel::ContractRunner*) thisPointer;
    return contractRunner->l_alloc_restricted(contractRunner->ud.get(), ptr, osize, nsize);
}

CryptoKernel::ContractRunner::~ContractRunner()
{
    delete blockchainInterface;
}

void* CryptoKernel::ContractRunner::l_alloc_restricted(void* ud, void* ptr, size_t osize, size_t nsize)
{
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

   if (nsize == 0){
     free(ptr);
     *used -= osize; /* substract old size from used memory */
     return NULL;
   }
   else
   {
     if (*used + (nsize - osize) > memoryLimit) {/* too much memory in use */
       throw std::runtime_error("Memory limit reached");
     }
     ptr = realloc(ptr, nsize);
     if (ptr) {/* reallocation successful? */
       *used += (nsize - osize);
     }
     return ptr;
   }
}

std::string CryptoKernel::ContractRunner::compile(const std::string contractScript)
{
    sel::State compilerState(true);

    if(!compilerState.Load("./compiler.lua"))
    {
        throw std::runtime_error("Failed to load compiler script");
    }

    const std::string compressedBytecode = compilerState["compile"](contractScript);

    return base64_encode((unsigned char*)compressedBytecode.c_str(), compressedBytecode.size());
}

void CryptoKernel::ContractRunner::setupEnvironment(Storage::Transaction* dbTx, const CryptoKernel::Blockchain::transaction tx, const CryptoKernel::Blockchain::output input)
{
    const int lim = this->pcLimit;
    (*state.get())["pcLimit"] = lim;

    (*state.get())["Crypto"].SetClass<CryptoKernel::Crypto, bool>("getPublicKey", &CryptoKernel::Crypto::getPublicKey,
                                                         "getPrivateKey", &CryptoKernel::Crypto::getPrivateKey,
                                                         "setPublicKey", &CryptoKernel::Crypto::setPublicKey,
                                                         "setPrivateKey", &CryptoKernel::Crypto::setPrivateKey,
                                                         "sign", &CryptoKernel::Crypto::sign,
                                                         "verify", &CryptoKernel::Crypto::verify,
                                                         "getStatus", &CryptoKernel::Crypto::getStatus
                                                        );
    (*state.get())["txJson"] = CryptoKernel::Storage::toString(CryptoKernel::Blockchain::transactionToJson(tx));
    (*state.get())["thisInputJson"] = CryptoKernel::Storage::toString(CryptoKernel::Blockchain::outputToJson(input));
    (*state.get())["outputSetId"] = CryptoKernel::Blockchain::calculateOutputSetId(tx.outputs);
    blockchainInterface->setTransaction(dbTx);
    (*state.get())["Blockchain"].SetObj((*blockchainInterface), "getBlock", &BlockchainInterface::getBlock, "getTransaction", &BlockchainInterface::getTransaction);
}

bool CryptoKernel::ContractRunner::evaluateValid(Storage::Transaction* dbTx, const CryptoKernel::Blockchain::transaction tx)
{
    CryptoKernel::Blockchain::transaction myTx = tx;

    std::vector<CryptoKernel::Blockchain::output>::iterator it;
    for(it = myTx.inputs.begin(); it < myTx.inputs.end(); it++)
    {
        if(!(*it).data["contract"].empty())
        {
            setupEnvironment(dbTx, tx, (*it));
            if(!(*state.get()).Load("./sandbox.lua"))
            {
                throw std::runtime_error("Failed to load sandbox.lua");
            }

            bool result = false;
            std::string errorMessage = "";
            sel::tie(result, errorMessage) = (*state.get())["verifyTransaction"](base64_decode((*it).data["contract"].asString()));

            if(errorMessage != "") {
                throw std::runtime_error(errorMessage);
            }

            return result;
        }
    }

    return true;
}
