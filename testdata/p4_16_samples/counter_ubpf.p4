#include <core.p4>
#include <ubpf_model.p4>

typedef bit<48>  EthernetAddress;

header ethernet_t {
    EthernetAddress dstAddr;
    EthernetAddress srcAddr;
    bit<16>         etherType;
}

struct metadata {
}

struct Headers_t {
    ethernet_t       ethernet;
}

parser prs(packet_in p, out Headers_t headers, inout metadata meta, inout standard_metadata std_meta) {
    state start {
        p.extract(headers.ethernet);
        transition accept;
    }
}

control pipe(inout Headers_t headers, inout metadata meta, inout standard_metadata std_meta) {

    Counter<bit<32>,bit<32>>(1024, CounterType.PACKETS) counter_a;
    Counter<bit<32>,bit<32>>(1024, CounterType.BYTES) counter_b;
    Counter<bit<32>,bit<32>>(1024, CounterType.PACKETS_AND_BYTES) counter_c;

    apply {
        counter_a.count(0);
        counter_a.count(10);
        counter_a.count(50);

        counter_b.count(0);
        counter_b.count(10);
        counter_b.count(50);

        counter_c.count(0);
        counter_c.count(10);
        counter_c.count(50);
    }

}

control dprs(packet_out packet, in Headers_t headers) {
    apply { packet.emit(headers.ethernet); }
}

ubpf(prs(), pipe(), dprs()) main;