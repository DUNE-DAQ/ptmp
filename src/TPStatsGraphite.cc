// Convert from PTMP JSON messages to Graphite line oriented strings

#include "ptmp/api.h"
#include "ptmp/internals.h"
#include "ptmp/factory.h"
#include "json.hpp"

#include <sstream>

PTMP_AGENT(ptmp::TPStatsGraphite, stats_graphite)

using json = nlohmann::json;

// fixme: internalize this
const int json_message_type = 0x4a534f4e; // 1246973774 in decimal

// We don't really want to support these as internal PTMP message
// types but if user doesn't specify a STREAM output socket then they
// are wanting the messages to stay inside ZeroMQ so we best protect
// ourselves.
const int graphite_message_type = 0x474c4f54; // 1196183380 in decimal

// Flatten a nested JSON object into lines containing a dot-separated
// key, a value and a Unix timestamp.
static
std::string stringify(json& jdat, const std::string& prefix,
                      time_t now_s)
{
    if (jdat.is_null()) {
        return "";
    }

    std::stringstream ss;
    if (jdat.is_object()) {
        if (jdat["now"].is_number()) {
            now_s = jdat["now"];
        }

        for (auto& el : jdat.items()) {
            json& jval = el.value();
            std::stringstream key;
            std::string thiskey = el.key();
            key << prefix << "." << thiskey;
            ss << stringify(jval, key.str(), now_s);
            if (thiskey == "ntpsperlink") {
                std::cout << prefix << "." << thiskey << " = " << jval << std::endl;
            }
        }
    }
    else if (jdat.is_array()) {
        for (size_t ind = 0; ind<jdat.size(); ++ind) {
            std::stringstream key;
            key << prefix << "." << ind;
            json& jele = jdat[ind];
            ss << stringify(jele, key.str(), now_s);
        }
    }
    else {                      // atom
        ss << prefix << " " << jdat << " " << now_s << "\n";
    }

    return ss.str();
}

static
void stats_graphite(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    std::string name = "stats_graphite";
    if (config["name"].is_string()) {
        name = config["name"];
    }
    ptmp::internals::set_thread_name(name);

    zsock_t* isock = NULL;
    if (!config["input"].is_null()) {
        std::string cfg = config["input"].dump();
        isock = ptmp::internals::endpoint(cfg);
        if (!isock) {
            throw std::runtime_error("no input socket configured");
        }
    }
    zsock_t* osock = NULL;
    if (!config["output"].is_null()) {
        std::string cfg = config["output"].dump();
        osock = ptmp::internals::endpoint(cfg);
        if (!osock) {
            throw std::runtime_error("no output socket configured");
        }
    }

    uint8_t oid [256];
    size_t oid_size = 256;
    if (streq(zsock_type_str(osock), "STREAM")) {
        int rc = zmq_getsockopt(zsock_resolve(osock), ZMQ_IDENTITY,
                                oid, &oid_size);
        assert(rc == 0);
    }
    else {
        zsys_warning("stats_graphite: not configured with STREAM output socket");
    }

    zsock_signal(pipe, 0); // signal ready    

    zpoller_t* poller = zpoller_new(pipe, isock, NULL);

    int count = 0;
    bool got_quit = false;
    while (!zsys_interrupted) {
        void* which = zpoller_wait(poller, -1);

        if (which == pipe) {
            zsys_info("stats_graphite: got quit");
            got_quit = true;
            goto cleanup;
        }

        int mtype=0;
        char* sdat=0;
        zsock_recv(isock, "is", &mtype, &sdat);
        if (mtype != json_message_type) {
            zsys_warning("stats_graphite: got unsupported message type: %d",
                         mtype);
            continue;
        }

        auto now = ptmp::data::now();
        time_t now_s = now/1000000;
        
        auto jdata = json::parse((const char*) sdat);
        freen(sdat);
        std::string sout = stringify(jdata, "ptmp.tpstats", now_s);
        if (sout.empty()) {
            continue;
        }

        zmsg_t* msg = zmsg_new();
        if (oid) {              // STREAM requires an ID for routing
            zmsg_addmem (msg, oid, oid_size);
        }
        else {                  // o.w. assume we are still in PTMP
            zmsg_addmem(msg, &graphite_message_type,
                        sizeof(graphite_message_type));
        }
        zmsg_addstr(msg, sout.c_str());
        zmsg_send(&msg, osock);
    }

  cleanup:
    zpoller_destroy(&poller);
    zsock_destroy(&isock);
    zsock_destroy(&osock);

}

ptmp::TPStatsGraphite::TPStatsGraphite(const std::string& config)
    : m_actor(zactor_new(stats_graphite, (void*)config.c_str()))
{
}

ptmp::TPStatsGraphite::~TPStatsGraphite()
{
    // actor may already have exited so sending this signal can hang.
    zsys_debug("stats_graphite: sending actor quit message");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
}
