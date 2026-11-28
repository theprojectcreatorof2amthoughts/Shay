import sys
import traceback

protocol_output = sys.stdout
namespace = {"__name__": "__main__"}

class ProtocolWriter:
    def write(self, value):
        if value:
            protocol_output.write("P" + str(value).encode("utf-8").hex() + "\\n")
            protocol_output.flush()
        return len(value)

    def flush(self):
        protocol_output.flush()

protocol_stream = ProtocolWriter()

def shay_input(prompt=""):
    protocol_output.write("I" + str(prompt).encode("utf-8").hex() + "\\n")
    protocol_output.flush()
    response = sys.stdin.readline()
    if not response:
        raise EOFError("input stream closed")
    return bytes.fromhex(response.strip()).decode("utf-8")

namespace["input"] = shay_input

for request in sys.stdin:
    request = request.strip()
    failed = False
    try:
        code = bytes.fromhex(request).decode("utf-8")
        old_stdout, old_stderr = sys.stdout, sys.stderr
        sys.stdout = protocol_stream
        sys.stderr = protocol_stream
        try:
            exec(compile(code, "<shay-python>", "exec"), namespace, namespace)
        finally:
            sys.stdout, sys.stderr = old_stdout, old_stderr
    except BaseException:
        failed = True
        traceback.print_exc(file=protocol_stream)

    sys.stdout.write("E\\n" if failed else "O\\n")
    sys.stdout.flush()
