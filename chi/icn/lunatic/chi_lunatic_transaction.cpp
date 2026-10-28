#include "chi_lunatic_transaction.hpp"

namespace CHI::Lunatic {

Transaction::Transaction(TransactionHandle handle, const Request& request, Time actionAge)
    : handle(handle), request(request), actionAge(actionAge)
{
}

}
