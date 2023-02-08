#include "byte_stream.hh"
// Dummy implementation of a flow-controlled in-memory byte stream.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) { _cap = capacity; }

size_t ByteStream::write(const string &data) {
    size_t len = data.size();
    if (remaining_capacity() < data.size()){
        len = remaining_capacity();
    }
    _buffer += data.substr(0, len);

    _writes += len;
    return len; 
}

size_t ByteStream::write(const char data) {
    if (remaining_capacity() <= 0){
        return 0;
    }
    _buffer += data;
    _writes++;
    return 1;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    return this->_buffer.substr(0, min(len, _buffer.size()));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    int temp = len;
    if (len > _buffer.size()) temp = _buffer.size();
    this->_reads += temp;
    this->_buffer.erase(0, temp);
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t actual_read_size = min(len, _buffer.size());
    std::string s = peek_output(actual_read_size);
    pop_output(actual_read_size);
    return s;
}

void ByteStream::end_input() {this->_input_ends = true;}

bool ByteStream::input_ended() const { return this->_input_ends; }

size_t ByteStream::buffer_size() const { return this->_buffer.size(); }

bool ByteStream::buffer_empty() const { return this->_buffer.size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && this->_input_ends; }

size_t ByteStream::bytes_written() const { return this->_writes; }

size_t ByteStream::bytes_read() const { return this->_reads; }

size_t ByteStream::remaining_capacity() const { return this->_cap - _buffer.size(); }