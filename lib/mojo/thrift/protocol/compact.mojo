from bit.bit import byte_swap
from memory.unsafe_pointer import UnsafePointer

from .base import TProtocol, TType
from ..transport import TTransport


@always_inline
fn zigzag_encode(n: Int64) -> UInt64:
    return UInt64((n << 1) ^ (n >> 63))

@always_inline
fn zigzag_decode(n: UInt64) -> Int64:
    return Int64((n >> 1) ^ -(n & 1))

fn encode_varint(n: UInt64) -> List[UInt8]:
    var buf = List[UInt8](capacity=8)
    var n_ = n
    while n_ > 0x7F:
        buf.append(UInt8(n_) | 0x80)
        n_ >>= 7

    buf.append(UInt8(n_))

    return buf

fn decode_varint(mut buf: List[UInt8]) -> Optional[(UInt64, Int)]:
    var n: UInt64 = 0
    var shift = 0
    var success = False

    for i in range(len(buf)):
        var byte = buf[i]
        n |= UInt64(byte & 0x7F) << shift
        shift += 7
        if byte & 0x80 == 0:
            success = True
            break
        if shift > (9 * 7):
            success = False
            break

    if success:
        return Optional((n, shift // 7))
    else:
        return None

fn encode_varint[T: DType](n: SIMD[T, 1]) raises -> List[UInt8]:
    @parameter
    if T.is_integral():
        if T.is_signed():
            var n_ = zigzag_encode(Int64(n))
            return encode_varint(n_)
        else:
            return encode_varint(UInt64(n))
    else:
        raise Error("Unsupported type for varint encoding")

fn decode_varint[T: DType](mut buf: List[UInt8]) raises -> Optional[(SIMD[T, 1], Int)]:
    var res = decode_varint(buf)
    @parameter
    if T.is_integral():
        if T.is_signed():
            if res:
                val, sz = res.value()
                return Optional((SIMD[T, 1](zigzag_decode(val)), sz))
            else:
                return None
        else:
            if res:
                val, sz = res.value()
                return Optional((SIMD[T, 1](val), sz))
            else:
                return None
    else:
        raise Error("Unsupported type for varint decoding")

fn type_to_u8(type: TType) raises -> UInt8:
    if type == TType.stop:
        return 0
    if type == TType.i8:
        return 3
    elif type == TType.i16:
        return 4
    elif type == TType.i32:
        return 5
    elif type == TType.i64:
        return 6
    elif type == TType.double:
        return 7
    elif type == TType.string:
        return 8
    elif type == TType.binary:
        return 8
    elif type == TType.list:
        return 9
    elif type == TType.struct_:
        return 12
    elif type == TType.union:
        return 12
    else:
        raise Error("Unsupported type for conversion to UInt8: " + String(type))

fn collection_type_to_u8(type: TType) raises -> UInt8:
    if type == TType.bool:
        return 1
    else:
        return type_to_u8(type)

fn u8_to_type(type: UInt8) raises -> TType:
    if type == 0:
        return TType.stop
    elif type == 1:
        return TType.bool
    elif type == 2:
        return TType.bool
    elif type == 3:
        return TType.i8
    elif type == 4:
        return TType.i16
    elif type == 5:
        return TType.i32
    elif type == 6:
        return TType.i64
    elif type == 7:
        return TType.double
    elif type == 8:
        return TType.string
    elif type == 9:
        return TType.list
    elif type == 12:
        return TType.struct_
    else:
        raise Error("Unsupported UInt8 for conversion to TType: " + String(type))

fn u8_to_collection_type(type: UInt8) raises -> TType:
    if type == 1:
        return TType.bool
    else:
        return u8_to_type(type)

@value
struct TCompactProtocol[Transport: TTransport](TProtocol):
    var trans: Transport
    var read_bool_value: Optional[Bool]
    var last_read_field_id: Int16
    var last_read_field_id_stack: List[Int16]
    var last_write_field_id: Int16
    var last_write_field_id_stack: List[Int16]
    var pending_field_id: Optional[Int16]

    fn write_field_header(mut self, field_type: UInt8, field_id: Int16) raises -> None:
        var field_delta = field_id - self.last_write_field_id
        if field_delta > 0 and field_delta < 16:
            self.write_byte((UInt8(field_delta) << 4) | field_type)
        else:
            self.write_byte(UInt8(field_type))
            self.write_i16(field_id)
        self.last_write_field_id = field_id

    fn read_byte(mut self) -> UInt8:
        var buf = self.trans.read_all(1)
        return buf[0]

    fn write_byte(mut self, byte: UInt8) -> None:
        self.trans.write(List[UInt8](byte))

    fn read_varint[T: DType](mut self) raises -> Optional[SIMD[T, 1]]:
        var n: UInt64 = 0
        var shift = 0
        var success = False

        for _ in range(T.sizeof()):
            var byte = self.read_byte()
            n |= UInt64(byte & 0x7F) << shift
            shift += 7
            if byte & 0x80 == 0:
                success = True
                break
            if shift > (9 * 7):
                success = False
                break

        @parameter
        if T.is_integral():
            if T.is_signed():
                if success:
                    return Optional(SIMD[T, 1](zigzag_decode(n)))
                else:
                    return None
            else:
                if success:
                    return Optional(SIMD[T, 1](n))
                else:
                    return None
        else:
            raise Error("Unsupported type for varint decoding")

    fn write_varint[T: DType](mut self, n: SIMD[T, 1]) raises -> None:
        # var n_: UInt64 = 0
        @parameter
        if T.is_integral():
            if T.is_signed():
                n_ = zigzag_encode(Int64(n))
            else:
                n_ = UInt64(n)
        else:
            raise Error("Unsupported type for varint encoding")

        # var buf = List[UInt8](capacity=8)
        while n_ > 0x7F:
            self.write_byte(UInt8(n_) | 0x80)
            n_ >>= 7

        self.write_byte(UInt8(n_))

    fn read_bool(mut self) raises -> Bool:
        if self.read_bool_value:
            # for field value
            return self.read_bool_value.take()
        else:
            # for element value
            var byte = self.read_byte()
            if byte == 1:
                return True
            elif byte == 2:
                return False
            else:
                raise Error("Invalid boolean value: " + String(byte))

    fn write_bool(mut self, bool: Bool) raises -> None:
        if self.pending_field_id:
            var field_id = self.pending_field_id.take()
            var field_type_u8: UInt8 = 0x01 if bool else 0x02
            self.write_field_header(field_type_u8, field_id)
        else:
            if bool:
                self.write_byte(UInt8(0x01))
            else:
                self.write_byte(UInt8(0x02))

    fn read_double(mut self) -> Float64:
        var buf = self.trans.read_all(8)
        # ToDo: check endian
        var ptr = buf.unsafe_ptr()
        var float64_ptr = ptr.bitcast[Float64]()
        return float64_ptr[]

    fn write_double(mut self, f64_val: Float64) -> None:
        var float64_ptr = UnsafePointer(to=f64_val)
        var uint8_ptr = float64_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=8)

        # ToDo: check endian
        for i in range(8):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)

    fn read_i8(mut self) -> Int8:
        return Int8(self.read_byte())

    fn write_i8(mut self, i8_val: Int8) -> None:
        self.write_byte(UInt8(i8_val))

    fn read_integral[T: DType](mut self) raises -> SIMD[T, 1]:
        var max_size = T.sizeof()
        var buf = List[UInt8](capacity=max_size)
        var length = 0
        while True:
            if length >= max_size:
                raise Error("Varint overflow")
            var byte = self.read_byte()
            buf.append(byte)
            length += 1
            if byte & 0x80 == 0:
                break

        var i_val = decode_varint[T](buf)
        if i_val:
            val, _ = i_val.value()
            return SIMD[T, 1](val)
        else:
            raise Error("Failed to decode varint")

    fn write_integral[T: DType](mut self, i_val: SIMD[T, 1]) raises -> None:
        var bytes = encode_varint[T](i_val)
        self.trans.write(bytes)

    fn read_i16(mut self) raises -> Int16:
        return self.read_integral[DType.int16]()

    fn write_i16(mut self, i16_val: Int16) raises -> None:
        self.write_integral[DType.int16](i16_val)

    fn read_i32(mut self) raises -> Int32:
        return self.read_integral[DType.int32]()

    fn write_i32(mut self, i32_val: Int32) raises -> None:
        self.write_integral[DType.int32](i32_val)

    fn read_i64(mut self) raises -> Int64:
        return self.read_integral[DType.int64]()

    fn write_i64(mut self, i64_val: Int64) raises -> None:
        self.write_integral[DType.int64](i64_val)

    fn read_binary(mut self) raises -> List[UInt8]:
        var res = self.read_varint[DType.uint32]()
        if res:
            var size = Int(res.value())
            # ToDo: check string length
            var buf = self.trans.read_all(size)
            return buf
        else:
            raise Error("Failed to read binary data")

    fn read_string(mut self) raises -> String:
        var bytes = self.read_binary()
        return String(bytes=bytes)

    fn write_binary(mut self, bytes: List[UInt8]) raises -> None:
        self.write_varint[DType.uint32](UInt32(len(bytes)))
        self.trans.write(bytes)

    fn write_string(mut self, str: String) raises -> None:
        var size = str.byte_length()
        var uint8_ptr = str.unsafe_ptr()
        var bytes = List[UInt8](capacity=size)
        for i in range(size):
            bytes.append(uint8_ptr[i])

        self.write_binary(bytes)

    fn read_struct_begin(mut self) -> None:
        self.last_read_field_id_stack.append(self.last_read_field_id)
        self.last_read_field_id = 0

    fn read_struct_end(mut self) -> None:
        self.last_read_field_id = self.last_read_field_id_stack.pop()

    fn read_field_begin(mut self) raises -> (String, TType, Int16):
        var first_byte = self.read_byte()
        var field_delta = (first_byte & 0xF0) >> 4
        var field_type_bits = first_byte & 0x0F
        var field_type = u8_to_type(field_type_bits)

        if field_type_bits == 0x01:
            self.read_bool_value = Optional(True)
        elif field_type_bits == 0x02:
            self.read_bool_value = Optional(False)

        if field_type == TType.stop:
            return (String(""), field_type, Int16(0))
        else: 
            if field_delta != 0:
                self.last_read_field_id += Int16(field_delta)
            else:
                self.last_read_field_id = self.read_i16()
            return (String(""), field_type, self.last_read_field_id)

    fn read_field_end(self) -> None:
        pass

    fn read_list_begin(mut self, ttype: TType) raises -> Int:
        var header = self.read_byte()
        var element_type = u8_to_collection_type(header & 0x0F)
        if element_type != ttype:
            raise Error("list element type expected" + String(ttype) + " got " + String(element_type))

        var maybe_size = (header & 0xF0) >> 4
        if maybe_size == 15:
            return Int(self.read_i32())
        else:
            return Int(maybe_size)

    fn read_list_end(self) -> None:
        pass

    fn write_struct_begin(mut self) -> None:
        self.last_write_field_id_stack.append(self.last_write_field_id)
        self.last_write_field_id = 0

    fn write_field_begin(mut self, _name: String, type: TType, id: Int16) raises -> None:
        if type == TType.bool:
            if self.pending_field_id:
                raise Error("Trying to set a pending field id, a value is already set.")
            self.pending_field_id = Optional(id)
        else:
            self.write_field_header(type_to_u8(type), id)

    fn write_field_end(self) raises -> None:
        if self.pending_field_id:
            raise Error("Write field ends while peding field id is not written for bool type.")

    fn write_field_stop(mut self) -> None:
        self.write_byte(UInt8(TType.stop.value))

    fn write_list_begin(mut self, ttype: TType, size: Int) raises -> None:
        var element_type = collection_type_to_u8(ttype)
        if size <= 14:
            var header = (UInt8(size) << 4) | element_type
            self.write_byte(header)
        else:
            var header = 0xF0 | element_type
            self.write_byte(header)
            self.write_i32(Int32(size))

    fn write_list_end(self) -> None:
        pass

    fn write_struct_end(mut self) raises -> None:
        if self.pending_field_id:
            raise Error("Write struct ends while peding field id is not written for bool type.")
        self.last_write_field_id = self.last_write_field_id_stack.pop()

    fn skip(self, ttype: TType) -> None:
        pass
