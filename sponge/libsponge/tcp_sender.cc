#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check_lab4`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

// my code here
void TCPSender::sending_operations(TCPSegment& seg){
    //push segment on to the _segments_out queue
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    _segments_wait.push(seg);
    // set timer
    if (!_timer_is_running){
        _timer = 0;
        _timer_is_running = true;
    }
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
}
//
uint64_t TCPSender::bytes_in_flight() const { 
    return _bytes_in_flight; 
}

void TCPSender::fill_window() {
    TCPSegment seg;
    
    if (_fin) return;
    if (!_syn){
        seg.header().syn = true;
        _syn = true;
        sending_operations(seg);
        return;
    }
    uint16_t window_size = 1;
    if (_receiver_window_size > 0) window_size = _receiver_window_size;
    // return fin
    if (_stream.eof() && _recieved_ackno + window_size > _next_seqno){
        seg.header().fin = true;
        _fin = true;
        sending_operations(seg);
        return;
    }
    // LOOP
    while(_recieved_ackno + window_size > _next_seqno && !_stream.buffer_empty()){
        size_t bytes_to_send = min(TCPConfig::MAX_PAYLOAD_SIZE, window_size + _recieved_ackno - _next_seqno);
        seg.payload() = Buffer(std::move(_stream.read(bytes_to_send)));
        if(_stream.eof() && seg.length_in_sequence_space() < window_size){
            seg.header().fin = true;
            _fin = true;
        }
        /*
        if (seg.length_in_sequence_space() == 0){
            return;
        }*/
        sending_operations(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    // DUMMY_CODE(ackno, window_size); 
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (abs_ackno > _next_seqno ) return; // out of range or has been recieved

    if(abs_ackno >= _recieved_ackno){
        _receiver_window_size = window_size;
        _recieved_ackno = abs_ackno;
    }

    bool fill_flag = false;
    while(! _segments_wait.empty()) {
        TCPSegment seg = _segments_wait.front(); 
        if(unwrap(seg.header().seqno, _isn, _next_seqno)+seg.length_in_sequence_space() > abs_ackno){
            return;
        }
        _bytes_in_flight -= seg.length_in_sequence_space();
        _segments_wait.pop();
        fill_flag = true;
        _retransmission_timeout = _initial_retransmission_timeout;
        _retransmission_times = 0;
        _timer = 0;
    }
    if (fill_flag) fill_window();
    _timer_is_running = !_segments_wait.empty();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    //DUMMY_CODE(ms_since_last_tick); 
    if (!_timer_is_running) return;
    _timer += ms_since_last_tick;
    // time out
    if (_timer>=_retransmission_timeout && !_segments_wait.empty()){
        _segments_out.push(_segments_wait.front());
        
        _timer_is_running = true;
        _timer = 0;

        if(_receiver_window_size > 0||_segments_wait.front().header().syn){
            _retransmission_timeout *= 2;
            _retransmission_times++; // record
        }
    }

}

unsigned int TCPSender::consecutive_retransmissions() const { 
    return _retransmission_times; 
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
