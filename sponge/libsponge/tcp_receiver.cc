#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    //DUMMY_CODE(seg);
    if (seg.header().syn || _isn != nullopt){
        if (seg.header().syn) _isn = seg.header().seqno;
        uint64_t absSeq = unwrap(seg.header().seqno + static_cast<int>(seg.header().syn), _isn.value(), _checkpoint);
        _reassembler.push_substring(seg.payload().copy(), absSeq-1, seg.header().fin);
        _checkpoint += seg.length_in_sequence_space();
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_isn == nullopt) return nullopt; 
    return wrap(stream_out().bytes_written() + stream_out().input_ended() + 1,  _isn.value());
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
