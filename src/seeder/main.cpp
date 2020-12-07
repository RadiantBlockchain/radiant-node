// Copyright (c) 2017-2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <clientversion.h>
#include <fs.h>
#include <logging.h>
#include <protocol.h>
#include <seeder/bitcoin.h>
#include <seeder/db.h>
#include <seeder/dns.h>
#include <streams.h>
#include <util/strencodings.h>
#include <util/system.h>

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <pthread.h>
#include <strings.h> // for strcasecmp

const std::function<std::string(const char *)> G_TRANSLATION_FUN = nullptr;

//! All globals in this file are private to this translation unit
namespace {

static const int CONTINUE_EXECUTION = -1;

static constexpr int DEFAULT_NUM_THREADS = 96;
static constexpr int DEFAULT_PORT = 53;
static constexpr int DEFAULT_NUM_DNS_THREADS = 4;
static constexpr bool DEFAULT_WIPE_BAN = false;
static constexpr bool DEFAULT_WIPE_IGNORE = false;
static const std::string DEFAULT_EMAIL = "";
static const std::string DEFAULT_NAMESERVER = "";
static const std::string DEFAULT_HOST = "";
static const std::string DEFAULT_TOR_PROXY = "";
static const std::string DEFAULT_IPV4_PROXY = "";
static const std::string DEFAULT_IPV6_PROXY = "";

class CDnsSeedOpts {
public:
    int nThreads            = DEFAULT_NUM_THREADS;
    int nPort               = DEFAULT_PORT;
    int nDnsThreads         = DEFAULT_NUM_DNS_THREADS;
    bool fWipeBan           = DEFAULT_WIPE_BAN;
    bool fWipeIgnore        = DEFAULT_WIPE_IGNORE;
    std::string mbox        = DEFAULT_EMAIL;
    std::string ns          = DEFAULT_NAMESERVER;
    std::string host        = DEFAULT_HOST;
    std::string tor         = DEFAULT_TOR_PROXY;
    std::string ipv4_proxy  = DEFAULT_IPV4_PROXY;
    std::string ipv6_proxy  = DEFAULT_IPV6_PROXY;
    std::set<uint64_t> filter_whitelist;

    int ParseCommandLine(int argc, char **argv) {
        SetupSeederArgs();
        std::string error;
        if (!gArgs.ParseParameters(argc, argv, error)) {
            std::fprintf(stderr, "Error parsing command line arguments: %s\n",
                         error.c_str());
            return EXIT_FAILURE;
        }
        if (HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
            std::string strUsage =
                PACKAGE_NAME " Seeder " + FormatFullVersion() + "\n";
            if (HelpRequested(gArgs)) {
                strUsage +=
                    "\nUsage:  bitcoin-seeder -host=<host> -ns=<ns> "
                    "[-mbox=<mbox>] [-threads=<threads>] [-port=<port>]\n\n" +
                    gArgs.GetHelpMessage();
            }

            std::fprintf(stdout, "%s", strUsage.c_str());
            return EXIT_SUCCESS;
        }

        nThreads = gArgs.GetArg("-threads", DEFAULT_NUM_THREADS);
        nPort = gArgs.GetArg("-port", DEFAULT_PORT);
        nDnsThreads = gArgs.GetArg("-dnsthreads", DEFAULT_NUM_DNS_THREADS);
        fWipeBan = gArgs.GetBoolArg("-wipeban", DEFAULT_WIPE_BAN);
        fWipeIgnore = gArgs.GetBoolArg("-wipeignore", DEFAULT_WIPE_IGNORE);
        mbox = gArgs.GetArg("-mbox", DEFAULT_EMAIL);
        ns = gArgs.GetArg("-ns", DEFAULT_NAMESERVER);
        host = gArgs.GetArg("-host", DEFAULT_HOST);
        tor = gArgs.GetArg("-onion", DEFAULT_TOR_PROXY);
        ipv4_proxy = gArgs.GetArg("-proxyipv4", DEFAULT_IPV4_PROXY);
        ipv6_proxy = gArgs.GetArg("-proxyipv6", DEFAULT_IPV6_PROXY);
        SelectParams(gArgs.GetChainName());

        if (gArgs.IsArgSet("-filter")) {
            // Parse whitelist additions
            std::string flagString = gArgs.GetArg("-filter", "");
            size_t flagstartpos = 0;
            while (flagstartpos < flagString.size()) {
                size_t flagendpos = flagString.find_first_of(',', flagstartpos);
                uint64_t flag = atoi64(flagString.substr(
                    flagstartpos, (flagendpos - flagstartpos)));
                filter_whitelist.insert(flag);
                if (flagendpos == std::string::npos) {
                    break;
                }
                flagstartpos = flagendpos + 1;
            }
        }
        if (filter_whitelist.empty()) {
            filter_whitelist.insert(NODE_NETWORK);
            filter_whitelist.insert(NODE_NETWORK | NODE_BLOOM);
            filter_whitelist.insert(NODE_NETWORK | NODE_XTHIN);
            filter_whitelist.insert(NODE_NETWORK | NODE_BLOOM | NODE_XTHIN);
        }
        return CONTINUE_EXECUTION;
    }

private:
    void SetupSeederArgs() {
        SetupHelpOptions(gArgs);
        gArgs.AddArg("-version", "Print version and exit", false,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-host=<host>", "Hostname of the DNS seed", false,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-ns=<ns>", "Hostname of the nameserver", false,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-mbox=<mbox>",
                     "E-Mail address reported in SOA records", false,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-threads=<threads>",
                     strprintf("Number of crawlers to run in parallel (default: %d)", DEFAULT_NUM_THREADS),
                     false, OptionsCategory::OPTIONS);
        gArgs.AddArg("-dnsthreads=<threads>",
                     strprintf("Number of DNS server threads (default: %d)", DEFAULT_NUM_DNS_THREADS), false,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-port=<port>", strprintf("UDP port to listen on (default: %d)", DEFAULT_PORT),
                     false, OptionsCategory::CONNECTION);
        gArgs.AddArg("-onion=<ip:port>", "Tor proxy IP/Port", false,
                     OptionsCategory::CONNECTION);
        gArgs.AddArg("-proxyipv4=<ip:port>", "IPV4 SOCKS5 proxy IP/Port",
                     false, OptionsCategory::CONNECTION);
        gArgs.AddArg("-proxyipv6=<ip:port>", "IPV6 SOCKS5 proxy IP/Port",
                     false, OptionsCategory::CONNECTION);
        gArgs.AddArg("-filter=<f1,f2,...>",
                     "Allow these flag combinations as filters", false,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-wipeban", strprintf("Wipe list of banned nodes (default: %d)", DEFAULT_WIPE_BAN), false,
                     OptionsCategory::CONNECTION);
        gArgs.AddArg("-wipeignore", strprintf("Wipe list of ignored nodes (default: %d)", DEFAULT_WIPE_IGNORE), false,
                     OptionsCategory::CONNECTION);
        SetupChainParamsBaseOptions();
    }
};

CAddrDb db;

extern "C" void *ThreadCrawler(void *data) {
    int *nThreads = (int *)data;
    do {
        std::vector<CServiceResult> ips;
        int wait = 5;
        db.GetMany(ips, 16, wait);
        int64_t now = std::time(nullptr);
        if (ips.empty()) {
            wait *= 1000;
            wait += std::rand() % (500 * *nThreads);
            Sleep(wait);
            continue;
        }

        std::vector<CAddress> addr;
        for (size_t i = 0; i < ips.size(); i++) {
            CServiceResult &res = ips[i];
            res.nBanTime = 0;
            res.nClientV = 0;
            res.nHeight = 0;
            res.strClientV = "";
            res.services = NODE_NONE;
            bool getaddr = res.ourLastSuccess + 86400 < now;
            res.fGood = TestNode(res.service, res.nBanTime, res.nClientV,
                                 res.strClientV, res.nHeight,
                                 getaddr ? &addr : nullptr,
                                 res.services);
        }

        db.ResultMany(ips);
        db.Add(addr);
    } while (1);
    return nullptr;
}

uint32_t GetIPList(void *thread, const char *requestedHostname,
                   addr_t *addr, uint32_t max, uint32_t ipv4,
                   uint32_t ipv6);

class CDnsThread {
public:
    struct FlagSpecificData {
        int nIPv4 = 0, nIPv6 = 0;
        std::vector<addr_t> cache;
        std::time_t cacheTime = 0;
        unsigned int cacheHits = 0;
    };

    dns_opt_t dns_opt; // must be first
    const int id;
    std::map<uint64_t, FlagSpecificData> perflag;
    std::atomic<uint64_t> dbQueries{0};
    std::set<uint64_t> filterWhitelist;

    void cacheHit(uint64_t requestedFlags, bool force = false) {
        static bool nets[NET_MAX] = {};
        if (!nets[NET_IPV4]) {
            nets[NET_IPV4] = true;
            nets[NET_IPV6] = true;
        }
        std::time_t now = std::time(nullptr);
        FlagSpecificData &thisflag = perflag[requestedFlags];
        thisflag.cacheHits++;
        if (force ||
            thisflag.cacheHits * 400 >
                (thisflag.cache.size() * thisflag.cache.size()) ||
            (thisflag.cacheHits * thisflag.cacheHits * 20 >
                 thisflag.cache.size() &&
             (now - thisflag.cacheTime > 5))) {
            std::set<CNetAddr> ips;
            db.GetIPs(ips, requestedFlags, 1000, nets);
            dbQueries++;
            thisflag.cache.clear();
            thisflag.nIPv4 = 0;
            thisflag.nIPv6 = 0;
            thisflag.cache.reserve(ips.size());
            for (auto &ip : ips) {
                struct in_addr addr;
                struct in6_addr addr6;
                if (ip.GetInAddr(&addr)) {
                    addr_t a;
                    a.v = 4;
                    std::memcpy(&a.data.v4, &addr, 4);
                    thisflag.cache.push_back(a);
                    thisflag.nIPv4++;
                } else if (ip.GetIn6Addr(&addr6)) {
                    addr_t a;
                    a.v = 6;
                    std::memcpy(&a.data.v6, &addr6, 16);
                    thisflag.cache.push_back(a);
                    thisflag.nIPv6++;
                }
            }
            thisflag.cacheHits = 0;
            thisflag.cacheTime = now;
        }
    }

    CDnsThread(CDnsSeedOpts *opts, int idIn) : id(idIn) {
        dns_opt.host = opts->host.c_str();
        dns_opt.ns = opts->ns.c_str();
        dns_opt.mbox = opts->mbox.c_str();
        dns_opt.datattl = 3600;
        dns_opt.nsttl = 40000;
        dns_opt.cb = GetIPList;
        dns_opt.port = opts->nPort;
        dns_opt.nRequests = 0;
        filterWhitelist = opts->filter_whitelist;
    }

    void run() { dnsserver(&dns_opt); }
};

uint32_t GetIPList(void *data, const char *requestedHostname, addr_t *addr,
                   uint32_t max, uint32_t ipv4, uint32_t ipv6) {
    CDnsThread *thread = (CDnsThread *)data;

    uint64_t requestedFlags = 0;
    int hostlen = std::strlen(requestedHostname);
    if (hostlen > 1 && requestedHostname[0] == 'x' &&
        requestedHostname[1] != '0') {
        char *pEnd;
        uint64_t flags = uint64_t(std::strtoull(requestedHostname + 1, &pEnd, 16));
        if (*pEnd == '.' && pEnd <= requestedHostname + 17 &&
            std::find(thread->filterWhitelist.begin(),
                      thread->filterWhitelist.end(),
                      flags) != thread->filterWhitelist.end()) {
            requestedFlags = flags;
        } else {
            return 0;
        }
    } else if (strcasecmp(requestedHostname, thread->dns_opt.host)) {
        return 0;
    }
    thread->cacheHit(requestedFlags);
    auto &thisflag = thread->perflag[requestedFlags];
    uint32_t size = thisflag.cache.size();
    uint32_t maxmax = (ipv4 ? thisflag.nIPv4 : 0) + (ipv6 ? thisflag.nIPv6 : 0);
    if (max > size) {
        max = size;
    }
    if (max > maxmax) {
        max = maxmax;
    }
    uint32_t i = 0;
    while (i < max) {
        uint32_t j = i + (std::rand() % (size - i));
        do {
            bool ok = (ipv4 && thisflag.cache[j].v == 4) ||
                      (ipv6 && thisflag.cache[j].v == 6);
            if (ok) {
                break;
            }
            j++;
            if (j == size) {
                j = i;
            }
        } while (1);
        addr[i] = thisflag.cache[j];
        thisflag.cache[j] = thisflag.cache[i];
        thisflag.cache[i] = addr[i];
        i++;
    }
    return max;
}

std::vector<CDnsThread *> dnsThreads;

extern "C" void *ThreadDNS(void *arg) {
    CDnsThread *thread = (CDnsThread *)arg;
    thread->run();
    return nullptr;
}

bool StatCompare(const CAddrReport &a, const CAddrReport &b) noexcept {
    if (a.uptime[4] == b.uptime[4]) {
        if (a.uptime[3] == b.uptime[3]) {
            return a.clientVersion > b.clientVersion;
        } else {
            return a.uptime[3] > b.uptime[3];
        }
    } else {
        return a.uptime[4] > b.uptime[4];
    }
}

extern "C" void *ThreadDumper(void *) {
    int count = 0;
    do {
        // First 100s, than 200s, 400s, 800s, 1600s, and then 3200s forever
        Sleep(100000 << count);
        if (count < 5) {
            count++;
        }

        {
            std::vector<CAddrReport> v = db.GetAll();
            std::sort(v.begin(), v.end(), StatCompare);
            FILE *f = fsbridge::fopen("dnsseed.dat.new", "w+");
            if (f) {
                {
                    CAutoFile cf(f, SER_DISK, CLIENT_VERSION);
                    cf << db;
                }
                std::rename("dnsseed.dat.new", "dnsseed.dat");
            }
            FILE *d = fsbridge::fopen("dnsseed.dump", "w");
            std::fprintf(d, "# address                                        good  "
                            "lastSuccess    %%(2h)   %%(8h)   %%(1d)   %%(7d)  "
                            "%%(30d)  blocks      svcs  version\n");
            double stat[5] = {0, 0, 0, 0, 0};
            for (CAddrReport rep : v) {
                std::fprintf(
                    d,
                    "%-47s  %4d  %11" PRId64
                    "  %6.2f%% %6.2f%% %6.2f%% %6.2f%% %6.2f%%  %6i  %08" PRIx64
                    "  %5i \"%s\"\n",
                    rep.ip.ToString().c_str(), (int)rep.fGood, rep.lastSuccess,
                    100.0 * rep.uptime[0], 100.0 * rep.uptime[1],
                    100.0 * rep.uptime[2], 100.0 * rep.uptime[3],
                    100.0 * rep.uptime[4], rep.blocks, rep.services,
                    rep.clientVersion, rep.clientSubVersion.c_str());
                stat[0] += rep.uptime[0];
                stat[1] += rep.uptime[1];
                stat[2] += rep.uptime[2];
                stat[3] += rep.uptime[3];
                stat[4] += rep.uptime[4];
            }
            std::fclose(d);
            FILE *ff = fsbridge::fopen("dnsstats.log", "a");
            std::fprintf(ff, "%llu %g %g %g %g %g\n",
                         (unsigned long long)(std::time(nullptr)), stat[0], stat[1],
                         stat[2], stat[3], stat[4]);
            std::fclose(ff);
        }
    } while (1);
    return nullptr;
}

extern "C" void *ThreadStats(void *) {
    bool first = true;
    do {
        char c[256];
        std::time_t tim = std::time(nullptr);
        struct tm *tmp = std::localtime(&tim);
        std::strftime(c, 256, "[%y-%m-%d %H:%M:%S]", tmp);
        CAddrDbStats stats;
        db.GetStats(stats);
        if (first) {
            first = false;
            std::fprintf(stdout, "\n\n\n\x1b[3A");
        } else {
            std::fprintf(stdout, "\x1b[2K\x1b[u");
        }
        std::fprintf(stdout, "\x1b[s");
        uint64_t requests = 0;
        uint64_t queries = 0;
        for (const auto *dnsThread : dnsThreads) {
            if (!dnsThread)
                continue;
            requests += dnsThread->dns_opt.nRequests;
            queries += dnsThread->dbQueries;
        }
        std::fprintf(stdout,
                     "%s %i/%i available (%i tried in %is, %i new, %i active), %i "
                     "banned; %llu DNS requests, %llu db queries\n",
                     c, stats.nGood, stats.nAvail, stats.nTracked, stats.nAge,
                     stats.nNew, stats.nAvail - stats.nTracked - stats.nNew,
                     stats.nBanned, (unsigned long long)requests,
                     (unsigned long long)queries);
        Sleep(1000);
    } while (1);
    return nullptr;
}

static constexpr unsigned int MAX_HOSTS_PER_SEED = 128;

extern "C" void *ThreadSeeder(void *) {
    do {
        for (const std::string &seed : Params().DNSSeeds()) {
            std::vector<CNetAddr> ips;
            LookupHost(seed.c_str(), ips, MAX_HOSTS_PER_SEED, true);
            for (auto &ip : ips) {
                db.Add(CAddress(CService(ip, GetDefaultPort()), ServiceFlags()),
                       true);
            }
        }
        Sleep(1800000);
    } while (1);
    return nullptr;
}

} // namespace

int main(int argc, char **argv) {
    // The logger dump everything on the console by default.
    LogInstance().m_print_to_console = true;

    std::signal(SIGPIPE, SIG_IGN);
    std::setbuf(stdout, nullptr);
    CDnsSeedOpts opts;
    int parseResults = opts.ParseCommandLine(argc, argv);
    if (parseResults != CONTINUE_EXECUTION) {
        return parseResults;
    }

    std::fprintf(stdout, "Supporting whitelisted filters: ");
    for (std::set<uint64_t>::const_iterator it = opts.filter_whitelist.begin();
         it != opts.filter_whitelist.end(); it++) {
        if (it != opts.filter_whitelist.begin()) {
            std::fprintf(stdout, ",");
        }
        std::fprintf(stdout, "0x%lx", (unsigned long)*it);
    }
    std::fprintf(stdout, "\n");
    if (!opts.tor.empty()) {
        CService service(LookupNumeric(opts.tor.c_str(), 9050));
        if (service.IsValid()) {
            std::fprintf(stdout, "Using Tor proxy at %s\n",
                         service.ToStringIPPort().c_str());
            SetProxy(NET_ONION, proxyType(service));
        }
    }
    if (!opts.ipv4_proxy.empty()) {
        CService service(LookupNumeric(opts.ipv4_proxy.c_str(), 9050));
        if (service.IsValid()) {
            std::fprintf(stdout, "Using IPv4 proxy at %s\n",
                         service.ToStringIPPort().c_str());
            SetProxy(NET_IPV4, proxyType(service));
        }
    }
    if (!opts.ipv6_proxy.empty()) {
        CService service(LookupNumeric(opts.ipv6_proxy.c_str(), 9050));
        if (service.IsValid()) {
            std::fprintf(stdout, "Using IPv6 proxy at %s\n",
                         service.ToStringIPPort().c_str());
            SetProxy(NET_IPV6, proxyType(service));
        }
    }
    bool fDNS = true;
    std::fprintf(stdout, "Using %s.\n", gArgs.GetChainName().c_str());
    if (opts.ns.empty()) {
        std::fprintf(stdout, "No nameserver set. Not starting DNS server.\n");
        fDNS = false;
    }
    if (fDNS && opts.host.empty()) {
        std::fprintf(stderr, "No hostname set. Please use -host.\n");
        return EXIT_FAILURE;
    }
    if (fDNS && opts.mbox.empty()) {
        std::fprintf(stderr, "No e-mail address set. Please use -mbox.\n");
        return EXIT_FAILURE;
    }
    FILE *f = fsbridge::fopen("dnsseed.dat", "r");
    if (f) {
        std::fprintf(stdout, "Loading dnsseed.dat...");
        CAutoFile cf(f, SER_DISK, CLIENT_VERSION);
        cf >> db;
        if (opts.fWipeBan) {
            db.banned.clear();
            std::fprintf(stdout, "Ban list wiped...");
        }
        if (opts.fWipeIgnore) {
            db.ResetIgnores();
            std::fprintf(stdout, "Ignore list wiped...");
        }
        std::fprintf(stdout, "done\n");
    }
    pthread_t threadDns, threadSeed, threadDump, threadStats;
    if (fDNS) {
        std::fprintf(stdout, "Starting %i DNS threads for %s on %s (port %i)...",
                     opts.nDnsThreads, opts.host.c_str(), opts.ns.c_str(),
                     opts.nPort);
        dnsThreads.reserve(opts.nDnsThreads);
        for (int i = 0; i < opts.nDnsThreads; i++) {
            dnsThreads.push_back(new CDnsThread(&opts, i));
            pthread_create(&threadDns, nullptr, ThreadDNS, dnsThreads.back());
            std::fprintf(stdout, ".");
            Sleep(20);
        }
        std::fprintf(stdout, "done\n");
    }
    std::fprintf(stdout, "Starting seeder...");
    pthread_create(&threadSeed, nullptr, ThreadSeeder, nullptr);
    std::fprintf(stdout, "done\n");
    std::fprintf(stdout, "Starting %i crawler threads...", opts.nThreads);
    pthread_attr_t attr_crawler;
    pthread_attr_init(&attr_crawler);
    pthread_attr_setstacksize(&attr_crawler, 0x20000);
    for (int i = 0; i < opts.nThreads; i++) {
        pthread_t thread;
        pthread_create(&thread, &attr_crawler, ThreadCrawler, &opts.nThreads);
    }
    pthread_attr_destroy(&attr_crawler);
    std::fprintf(stdout, "done\n");
    pthread_create(&threadStats, nullptr, ThreadStats, nullptr);
    pthread_create(&threadDump, nullptr, ThreadDumper, nullptr);
    void *res;
    pthread_join(threadDump, &res);
    return EXIT_SUCCESS;
}
