#pragma once
#include "account.h"
#include "provider.h"
#include <memory>

// The only place that knows which provider modules exist. A new vendor is
// added by including its header here and appending one entry to the table.
namespace Registry {
    struct TypeInfo {
        const char* id;
        const char* name;
    };

    const std::vector<TypeInfo>& types();
    bool isKnownType(const String& typeId);

    std::unique_ptr<Provider> create(const String& typeId);
    std::unique_ptr<Provider> fromRecord(const AccountRecord& rec);
}
