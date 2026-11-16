#include "security.h"

bool parseSecurityList(const string& list, SecurityOptions& out, string& bad) {
    string cur;
    auto apply = [&](const string& n) {
        if (n == "all") {
            out.statik = out.bounds = out.nullchk = out.divzero = out.overflow = true;
            out.canary = out.heap = out.recursion = out.harden = true;
        } else if (n == "static") out.statik = true;
        else if (n == "bounds") out.bounds = true;
        else if (n == "null") out.nullchk = true;
        else if (n == "div") out.divzero = true;
        else if (n == "overflow") out.overflow = true;
        else if (n == "canary") out.canary = true;
        else if (n == "heap") out.heap = true;
        else if (n == "recursion") out.recursion = true;
        else if (n == "harden") out.harden = true;
        else if (n == "none" || n.empty()) {
        } else {
            bad = n;
            return false;
        }
        return true;
    };
    for (size_t i = 0; i <= list.size(); i++) {
        if (i == list.size() || list[i] == ',') {
            if (!apply(cur)) return false;
            cur.clear();
        } else {
            cur += list[i];
        }
    }
    return true;
}

const char* hardenLinkFlags() {
    return "-fPIE -pie -Wl,-z,relro,-z,now,-z,noexecstack";
}
