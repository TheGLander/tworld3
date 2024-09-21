from ctypes import (
    CDLL,
    Structure,
    Union,
    c_bool,
    c_char_p,
    c_uint16,
    c_void_p,
    create_string_buffer,
)


libchips = CDLL("libchips.so")


class LibchipsError(Exception):
    pass


def Result(val_type: type):
    class Result_union(Union):
        _fields_ = [("value", val_type), ("error", c_char_p)]

    class Result_struct(Structure):
        _fields_ = [("success", c_bool), ("anon", Result_union)]
        _anonymous_ = ("anon",)

        def raise_if_error(self):
            if self.success:
                return
            raise LibchipsError(self.error.decode("ascii"))

    Result_struct.__name__ = f"Result_{str(val_type)}"

    return Result_struct

class Ruleset(c_void_p):
    @property
    def id(self):
        return libchips.Ruleset_get_id(self)

lynx_logic = Ruleset.in_dll(libchips, "lynx_logic")

libchips.LevelMetadata_get_title.restype=c_char_p
libchips.LevelMetadata_get_level_number.restype=c_uint16
libchips.LevelMetadata_get_time_limit.restype=c_uint16
libchips.LevelMetadata_get_chips_required.restype=c_uint16
libchips.LevelMetadata_get_password.restype=c_char_p
libchips.LevelMetadata_get_hint.restype=c_char_p
libchips.LevelMetadata_get_author.restype=c_char_p

class LevelMetadata(c_void_p):
    @property
    def title(self) -> str|None:
        return libchips.LevelMetadata_get_title(self).decode("latin-1")
    @property
    def level_number(self) -> int:
        return libchips.LevelMetadata_get_level_number(self)
    @property
    def time_limit(self) -> int:
        return libchips.LevelMetadata_get_time_limit(self)
    @property
    def chips_required(self) -> int:
        return libchips.LevelMetadata_get_chips_required(self)
    @property
    def password(self) -> str|None:
        return libchips.LevelMetadata_get_password(self).decode("latin-1")
    @property
    def hint(self) -> str|None:
        return libchips.LevelMetadata_get_hint(self).decode("latin-1")
    @property
    def author(self) -> str|None:
        return libchips.LevelMetadata_get_author(self).decode("latin-1")

libchips.LevelSet_get_levels_n.restype = c_uint16
libchips.LevelSet_get_level.restype = LevelMetadata

class LevelSet(c_void_p):
    @property
    def levels_n(self) -> int:
        return libchips.LevelSet_get_levels_n(self)
    def get_level(self, idx: int) -> LevelMetadata:
        return libchips.LevelSet_get_level(self, idx)
    @property
    def levels(self):
        for level_idx in range(self.levels_n):
            yield self.get_level(level_idx)


libchips.parse_ccl.restype = Result(LevelSet)

def parse_ccl(data: bytes) -> LevelSet:
    levelset_bytes = create_string_buffer(data)
    parse_result = libchips.parse_ccl(levelset_bytes, len(levelset_bytes) - 1)
    parse_result.raise_if_error()
    return parse_result.value
