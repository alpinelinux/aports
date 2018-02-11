from hypothesis import given
from hypothesis.strategies import text

@given(s=text())
def test_decode_inverts_encode(s):
    assert s.encode("UTF").decode("UTF-8") == s
