// A high level configuration relevant to running at PDSP



{
    cfg: {                      // PTMP component configuration that isn't socket related
        tspan: 50/0.02,         // window span in data time
        tbuf: 5000/0.02,        // turn around buffer in data time
        toffset: 0,
        sync_time: 10, // real time in ms to wait for tardy input streams
        detid: -1,              // use what's in TPSet
        tardy_policy:"drop",    // anything else destroys output ordering

        // Force replayed tpsets to get recent past tstarts based on
        // current real time.  Note, if set true, this will almost
        // certainly mess up inter-message sync based on this tstart.
        // It may both hide and introduce inter-link time shifts.
        // Only use this if there is some absolute need to pretend
        // like the data time is not what it really is.
        rewrite_tstart: 0,

        // time in ms for integrating link-level and channel-level stats
        link_integration_time:10000,
        chan_integration_time:20000,
    },

    outfiles: {
        tps: "pdsp-tps.dump",
        tcs: "pdsp-tcs.dump",
        tds: "pdsp-tds.dump",
    },


    // meta parameters
    //apas: std.range(1,6),       // Enumerate APA installation numbers to consider
    apas: [5,6],                // limited to what FELIX is hooked up to

    // socket parameters

    pubsub_hwm: 10000,      // high-water mark for pub/sub sockets

    // The PUB/SUB socket addresses, indexed by APA installation number (0 is undefined)
    addresses : {
        tps: [
            [null],             // index by APA number.  There is no APA 0
            // these are invented
            ["tcp://10.73.136.61:%d" % (15100+ind) for ind in std.range(1,10)], // apa1
            ["tcp://10.73.136.62:%d" % (15110+ind) for ind in std.range(1,10)], // apa2
            ["tcp://10.73.136.63:%d" % (15120+ind) for ind in std.range(1,10)], // apa3
            ["tcp://10.73.136.64:%d" % (15130+ind) for ind in std.range(1,10)], // apa4

            // these are confirmed in use by Phil
            ["tcp://10.73.136.67:%d" % (15140+ind) for ind in std.range(1,10)], // apa5
            ["tcp://10.73.136.60:%d" % (15150+ind) for ind in std.range(1,10)], // apa6
        ],
        tc: [
            null,           // index by APA number.  There is no APA 0
            // fixme: these are all invented
            "tcp://10.73.136.51:7771", // apa1:
            "tcp://10.73.136.52:7772", // apa2:
            "tcp://10.73.136.53:7773", // apa3:
            "tcp://10.73.136.54:7774", // apa4:
            // These are confirmed
            "tcp://10.73.136.31:40501", // apa5:
            "tcp://10.73.136.32:40502", // apa6:
        ],
        // Confirmed
        td: "tcp://10.73.136.32:7124", 

        stats: "tcp://127.0.0.1:6666",

    },
}



/*  From Phil: the current PUB addresses for TP sources (FELIX_BR) at PDSP
# APA 5
felix501: "tcp://10.73.136.67:15141"
felix502: "tcp://10.73.136.67:15142"
felix503: "tcp://10.73.136.67:15143"
felix504: "tcp://10.73.136.67:15144"
felix505: "tcp://10.73.136.67:15145"
felix506: "tcp://10.73.136.67:15146"
felix507: "tcp://10.73.136.67:15147"
felix508: "tcp://10.73.136.67:15148"
felix509: "tcp://10.73.136.67:15149"
felix510: "tcp://10.73.136.67:15150"
# APA 6
felix601: "tcp://10.73.136.60:15151"
felix602: "tcp://10.73.136.60:15152"
felix603: "tcp://10.73.136.60:15153"
felix604: "tcp://10.73.136.60:15154"
felix605: "tcp://10.73.136.60:15155"
felix606: "tcp://10.73.136.60:15156"
felix607: "tcp://10.73.136.60:15157"
felix608: "tcp://10.73.136.60:15158"
felix609: "tcp://10.73.136.60:15159"
felix610: "tcp://10.73.136.60:15160"
*/
