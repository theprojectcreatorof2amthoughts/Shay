import io
import sys
import traceback
from contextlib import redirect_stdout, redirect_stderr

namespace = {"__name__": "__main__"}
protocol_output = sys.stdout

def shay_input(prompt=""):
    protocol_output.write("I" + str(prompt).encode("utf-8").hex() + "\n")
    protocol_output.flush()
    response = sys.stdin.readline()
    if not response:
        raise EOFError("input stream closed")
    return bytes.fromhex(response.strip()).decode("utf-8")

namespace["input"] = shay_input

for request in sys.stdin:
    request = request.strip()
    output = io.StringIO()
    failed = False
    try:
        code = bytes.fromhex(request).decode("utf-8")
        with redirect_stdout(output), redirect_stderr(output):
            exec(compile(code, "<shay-python>", "exec"), namespace, namespace)
    except BaseException:
        failed = True
        traceback.print_exc(file=output)

    payload = output.getvalue().encode("utf-8").hex()
    sys.stdout.write(("E" if failed else "O") + payload + "\n")
    sys.stdout.flush()
