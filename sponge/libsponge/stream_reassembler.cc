
#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) ,
 _recorder(capacity, false), _datas(capacity, '\0')
{}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
//    DUMMY_CODE(data, index, eof);
    if (index >= _next + _output.remaining_capacity()){
        return;
    }
    if (eof && index + data.size() <= _next + _output.remaining_capacity()){
        _eof = true;
    }
    if (index + data.size() > _next){
        size_t temp = max(index, _next);
        for(size_t i = temp; i < index + data.size()&&i < _output.remaining_capacity()+_next;i++){
         
            if (_recorder[i-_next] == false){
                _datas[i-_next] = data[i-index];
                _recorder[i-_next] = true;
                _unassembled_bytes++;
            }
        }
        string msg;
        while(_recorder.front()){
            msg += _datas.front();
            _datas.pop_front();
            _datas.push_back('\0');
            _recorder.pop_front();
            _recorder.push_back(false);
        }
        if (msg.size() > 0){
            _unassembled_bytes -= msg.size();
            _next += msg.size();
            _output.write(msg);
        }
    }

    if  (_eof && empty()){
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
