trait TTransport(Copyable, Movable):
    fn read_all(mut self, n: Int) -> List[UInt8]:
        ...
    fn write(mut self, bytes: List[UInt8]) -> None:
        ...
    fn get_value(self) -> List[UInt8]:
        ...

@value
struct TMemoryBuffer(Writable, Stringable, TTransport):
    var buffer: List[UInt8]
    var offset: Int

    fn __init__(out self, buffer: List[UInt8]):
        self.buffer = buffer
        self.offset = 0

    fn read_all(mut self, n: Int) -> List[UInt8]:
        # ToDo: check self.buffer index out of range
        var buf = List[UInt8](capacity=n)
        for i in range(n):
            buf.append(self.buffer[self.offset + i])
        self.offset += n
        return buf

    fn write(mut self, bytes: List[UInt8]) -> None:
        for i in range(len(bytes)):
            self.buffer.append(bytes[i])

    fn get_value(self) -> List[UInt8]:
        return self.buffer

    fn __str__(self) -> String:
        return String.write(self)

    fn write_to[W: Writer](self, mut writer: W):
        writer.write("[")
        for e in self.buffer:
            writer.write(e[], ", ")
        writer.write("]")

