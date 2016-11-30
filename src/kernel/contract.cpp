#include "contract.h"

CryptoKernel::ContractRunner::ContractRunner(const CryptoKernel::Blockchain* blockchain, const uint64_t memoryLimit, const uint64_t instructionLimit)
{
    this->memoryLimit = memoryLimit;

    ud.reset(new int);
    *ud.get() = 0;
    lua_State* _l = lua_newstate(&CryptoKernel::ContractRunner::allocWrapper, this);
    luaL_openlibs(_l);
    state.reset(new sel::State(_l));

    setupEnvironment(state.get());
}

void* CryptoKernel::ContractRunner::allocWrapper(void* thisPointer, void* ptr, size_t osize, size_t nsize)
{
    CryptoKernel::ContractRunner* contractRunner = (CryptoKernel::ContractRunner*) thisPointer;
    return contractRunner->l_alloc_restricted(contractRunner->ud.get(), ptr, osize, nsize);
}

CryptoKernel::ContractRunner::~ContractRunner()
{

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
       return NULL;
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
    setupEnvironment(&compilerState);

    if(!compilerState.Load("./compiler.lua"))
    {
        throw std::runtime_error("Failed to load compiler script");
    }

    const std::string compressedBytecode = compilerState["compile"](contractScript);

    return compressedBytecode;
}

void CryptoKernel::ContractRunner::setupEnvironment(sel::State* stateEnv)
{

}

bool CryptoKernel::ContractRunner::evaluateValid(const CryptoKernel::Blockchain::transaction tx)
{
    CryptoKernel::Blockchain::transaction myTx = tx;

    std::vector<CryptoKernel::Blockchain::output>::iterator it;
    for(it = myTx.inputs.begin(); it < myTx.inputs.end(); it++)
    {
        if(!(*it).data["contract"].empty())
        {
            if(!(*state.get()).Load("./sandbox.lua"))
            {
                throw std::runtime_error("Failed to load sandbox.lua");
            }
            if(!(*state.get())["verifyTransaction"]((*it).data["contract"].asString()))
            {
                return false;
            }
        }
    }

    return true;
}
