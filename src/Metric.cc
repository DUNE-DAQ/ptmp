#include "ptmp/metrics.h"

#include "ptmp/api.h"
#include "ptmp/data.h"
#include "ptmp/internals.h"
#include "ptmp/stringutil.h"

#include "json.hpp"

#include <sstream>

using json = nlohmann::json;



// Flatten a nested JSON object into lines containing a dot-separated
// key, a value and a Unix timestamp.

std::string ptmp::metrics::glot_stringify(json& jdat, const std::string& prefix,
                                          ptmp::data::real_time_t now_us)
{
    time_t now_s = now_us / 1000000;
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
            ss << glot_stringify(jval, key.str(), now_s);
            // if (thiskey == "ntpsperlink") {
            //     std::cout << prefix << "." << thiskey << " = " << jval << std::endl;
            // }
        }
    }
    else if (jdat.is_array()) {
        for (size_t ind = 0; ind<jdat.size(); ++ind) {
            std::stringstream key;
            key << prefix << "." << ind;
            json& jele = jdat[ind];
            ss << glot_stringify(jele, key.str(), now_s);
        }
    }
    else {                      // atom
        ss << prefix << " " << jdat << " " << now_s << "\n";
    }

    return ss.str();
}
static
std::string make_prefix_dot(json& topcfg)
{
    std::string ret = "ptmp.";
    if ( topcfg["prefix"].is_string() ) {
        std::string mid = topcfg["prefix"];
        mid = ptmp::stringutil::trim(mid, ".");
        ret += mid + ".";
    }
    return ret;
}

static
std::string make_prefix_slash(json& topcfg)
{
    std::string ret = make_prefix_dot(topcfg);
    ptmp::stringutil::find_replace(ret, ".", "/");
    return "/" + ret;
}


static
zsock_t* make_sock(json& topcfg)
{
    if (!topcfg["output"].is_null()) {
        std::string cfg = topcfg["output"].dump();
        zsock_t* sock = ptmp::internals::endpoint(cfg);
        if (sock) {
            zsys_info("metric: osock: %s", cfg.c_str());
            return sock;
        }
    }
    throw std::runtime_error("metric: failed to make socket");
}


struct JsonProto : ptmp::metrics::Metric::Proto {

    std::string prefix;
    zsock_t* sock;

    JsonProto(json& cfg) : prefix(make_prefix_slash(cfg)), sock(make_sock(cfg)) {
        
    }
    ~JsonProto() {
        zsock_destroy(&sock);
    }

    std::string pathify(std::string path) {
        ptmp::stringutil::find_replace(path, ".", "/");
        return prefix + path;
    }

    void send(json& m, ptmp::data::real_time_t now_us) {
        if (!now_us) {
            now_us = ptmp::data::now();
        }
        json j;
        j[json::json_pointer(prefix)] = m;
        std::string s = j.dump();
        zsock_send(sock, "is", ptmp::metrics::json_message_type, s.c_str());
    }
};


struct GlotProto : ptmp::metrics::Metric::Proto {
    std::string prefix;
    zsock_t* sock;
    uint8_t oid [256];
    size_t oid_size = 256;

    GlotProto(json& cfg) : prefix(make_prefix_dot(cfg)), sock(make_sock(cfg)) {
        if (streq(zsock_type_str(sock), "STREAM")) {
            int rc = zmq_getsockopt(zsock_resolve(sock), ZMQ_IDENTITY,
                                    oid, &oid_size);
            assert(rc == 0);
        }
        else {
            zsys_warning("metrics: GLOT protocol requested but not using a STREAM socket");
        }
    }
    ~GlotProto() {
        zsock_destroy(&sock);
    }

    std::string pathify(std::string path) {
        ptmp::stringutil::find_replace(path, "/", ".");
        return prefix + path;
    }
    void send(json& m, ptmp::data::real_time_t now_us) {
        if (!now_us) { now_us = ptmp::data::now(); }

        zmsg_t* msg = zmsg_new();
        if (oid) {              // STREAM requires an ID for routing
            zmsg_addmem (msg, oid, oid_size);
        }
        else {                  // o.w. assume we are still in PTMP
            zmsg_addmem(msg, &ptmp::metrics::glot_message_type,
                        sizeof(ptmp::metrics::glot_message_type));
        }

        std::string sout = ptmp::metrics::glot_stringify(m, prefix, now_us);

        zmsg_addstr(msg, sout.c_str());
        zmsg_send(&msg, sock);
    }
};


ptmp::metrics::Metric::Metric(const std::string& config)
    : m_proto{NULL}
{
    json jcfg = json::parse(config);

    std::string proto = "JSON";
    if ( jcfg["proto"].is_string() ) {
        proto = jcfg["proto"];
    }
    if (proto == "JSON") {
        m_proto = new JsonProto(jcfg);
    }
    else if (proto == "GLOT") {
        m_proto = new GlotProto(jcfg);
    }
    else {
        throw std::runtime_error("unknown metric protocol: " + proto);
    }

}

ptmp::metrics::Metric::~Metric()
{
    delete m_proto; m_proto=0;
}

void ptmp::metrics::Metric::operator()(json& metrics, ptmp::data::real_time_t now_us)
{
    m_proto->send(metrics, now_us);
}

