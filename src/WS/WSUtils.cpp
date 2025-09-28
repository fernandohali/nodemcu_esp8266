#include "WSUtils.h"
#include "../Wifi/wifi.h" // para Net::ip()

namespace WSUtils
{

    bool isSelfHost(const char *host)
    {
        if (!host || !*host)
            return false;
        String self = Net::ip();
        return self.length() > 0 && self.equals(host);
    }

    void rotateHost(const char **hosts, size_t count, size_t &currentIndex, bool avoidSelf)
    {
        if (count <= 1)
            return;
        size_t start = currentIndex;
        do
        {
            currentIndex = (currentIndex + 1) % count;
            const char *h = hosts[currentIndex];
            if (h && *h)
            {
                if (!avoidSelf || !isSelfHost(h))
                    break;
            }
        } while (currentIndex != start);
    }
}
