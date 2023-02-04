#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection::send() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
    // input ends then close connection 
    if (_receiver.stream_out().input_ended()) {
        if (!_sender.stream_in().eof()) {
            _linger_after_streams_finish = false;
        }

        else if (_sender.bytes_in_flight() == 0) {
            if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
                _active = false;
            }
        }
    }
}

void TCPConnection::reset_connection() {

    TCPSegment seg;
    seg.header().rst = true;
    _segments_out.push(seg); // send RST

    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_seg_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    if (!_active) return;
    // reset connect time
    _time_since_last_seg_received = 0;
    // if RST then set errors
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
    }
    // CLOSED/LISTEN
    else if (_sender.next_seqno_absolute() == 0) {
        // get SYN，
        if (seg.header().syn) {
            _receiver.segment_received(seg);
            // send SYN
            connect();
        }
    }
    // SYN-sent
    else if (_sender.next_seqno_absolute() == _sender.bytes_in_flight() && !_receiver.ackno().has_value()) {
        if (seg.header().syn && seg.header().ack) {
            // get ACK and SYN, indicating that reciever actively starts the connection
            // Established
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _receiver.segment_received(seg);
            _sender.send_empty_segment(); // to send ACK
            send();
        } else if (seg.header().syn && !seg.header().ack) {
            // only get SYN，indicating connection starts at the same time
            _receiver.segment_received(seg);
            _sender.send_empty_segment();
            send();
        }
    }
    // SYN-Revd，input won't end
    else if (_sender.next_seqno_absolute() == _sender.bytes_in_flight() && _receiver.ackno().has_value() &&
             !_receiver.stream_out().input_ended()) {
        // get ACK
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
    }
    // Established
    else if (_sender.next_seqno_absolute() > _sender.bytes_in_flight() && !_sender.stream_in().eof()) {

        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        if (seg.length_in_sequence_space() > 0) {
            _sender.send_empty_segment();
        } //refresh ACK
        _sender.fill_window();
        send();
    }
    // Fin-Wait-1
    else if (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
             _sender.bytes_in_flight() > 0 && !_receiver.stream_out().input_ended()) {
        // get Fin
        if (seg.header().fin) {
            // Closing/Time-Wait
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _receiver.segment_received(seg);
            _sender.send_empty_segment(); // send new ACK
            send();
        } else if (seg.header().ack) {
            // get ACK, turn into Fin-Wait-2 status
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _receiver.segment_received(seg);
            send();
        }
    }
    // Fin-Wait-2
    else if (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
             _sender.bytes_in_flight() == 0 && !_receiver.stream_out().input_ended()) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        _sender.send_empty_segment();
        send();
    }
    // Time-Wait
    else if (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
             _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()) {
        if (seg.header().fin) {
            // get FIN，stay Time-Wait status
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _receiver.segment_received(seg);
            _sender.send_empty_segment();
            send();
        }
    }
    // other statuses
    else {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        _sender.fill_window();
        send();
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (data.empty()) return 0;
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    send();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active) return;
    // save the time and inform sender
    _time_since_last_seg_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // timeout->reset connection
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        reset_connection();
        return;
    }
    send();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send();
}



TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            reset_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
