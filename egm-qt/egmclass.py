from ctypes import *
from ctypes.util import find_library

##
# Some egmoney.h constants
# Cast them to whatever c_type is needed by the function

# error numbers
EGM_DBORIG = 1
EGM_DBFAIL = 2
EGM_ERRNO = 3
EGM_INVAL = 4
EGM_EACCT = 5
EGM_EFULL = 5

# ranges
EGM_RANGE   = 0
EGM_YEAR    = 1
EGM_QUARTER = 2
EGM_MONTH   = 3
EGM_ALLTIME = 4

# style
EGM_HTML  = 0
EGM_CSV   = 1
EGM_JSON  = 2
EGM_TERM  = 3

try:
    libname = find_library("egmoney")
    lib = CDLL(libname)

    Egmoney = c_void_p
    Egentry = c_void_p

    egm_errno = lib.egm_errno

    egm_perror = lib.egm_perror
    egm_perror.argtypes = [c_char_p]

    egm_init = lib.egm_init
    egm_init.restype = Egmoney

    egm_exit = lib.egm_exit
    egm_exit.argtypes = [Egmoney]

    # TODOD: Define struct tm for egm_set_range

    egm_readdb = lib.egm_readdb
    egm_readdb.restype = c_int
    egm_readdb.argtypes = [Egmoney, c_char_p, c_int]

    egm_path = lib.egm_path
    egm_path.restype = c_char_p
    egm_path.argtypes = [Egmoney]

    egm_next_entry = lib.egm_next_entry
    egm_next_entry.restype = Egentry
    egm_next_entry.argtypes = [Egmoney, Egentry]

    egm_new_entry = lib.egm_new_entry
    egm_new_entry.restype = Egentry
    egm_new_entry.argtypes = [c_ulong]

    egm_account_has_child = lib.egm_account_has_child
    egm_account_has_child.restype = c_int
    egm_account_has_child.argtypes = [Egmoney, c_int]

    egm_acctstr = lib.egm_acctstr
    egm_acctstr.argtypes = [Egmoney, c_int]
    egm_acctstr.restype = c_char_p

    egm_account_value = lib.egm_account_value
    egm_account_value.restype = c_float
    egm_account_value.argtypes = [Egmoney, c_int]

    egm_account_get_child = lib.egm_account_get_child
    egm_account_get_child.restype = c_int
    egm_account_get_child.argtypes = [Egmoney, c_int, c_int]

    egm_entry_valid = lib.egm_entry_valid
    egm_entry_valid.restype = c_int
    egm_entry_valid.argtypes = [Egentry]

    egm_entry_date = lib.egm_entry_date
    egm_entry_date.restype = c_ulong
    egm_entry_date.argtypes = [Egentry]

    egm_entry_stamp = lib.egm_entry_stamp
    egm_entry_stamp.restype = c_ulong
    egm_entry_stamp.argtypes = [Egentry]

    egm_entry_balanced = lib.egm_entry_balanced
    egm_entry_balanced.restype = c_int
    egm_entry_balanced.argtypes = [Egentry]

    egm_entry_add_notes = lib.egm_entry_add_notes
    egm_entry_add_notes.argtypes = [Egentry, c_char_p]

    egm_entry_add_trans = lib.egm_entry_add_trans
    egm_entry_add_trans.restype = c_int
    egm_entry_add_trans.argtypes = [Egmoney, Egentry, c_int, c_float]

    egm_entry_get_trans = lib.egm_entry_get_trans
    egm_entry_get_trans.restype = c_int
    egm_entry_get_trans.argtypes = [Egmoney, c_int, c_void_p, c_void_p]

    egm_refresh_balances = lib.egm_refresh_balances
    egm_refresh_balances.restype = c_int
    egm_refresh_balances.argtimes = [Egmoney]

except (AttributeError, TypeError):
   raise ImportError("Can't import " + __name__ + " due to shared library error.")


def addquot(s):
    return '"' + s + '"'

class EgmAccount:
    def __init__(self, egm, name, acctno):
        # XXX: Inerit lib and egm?
        self.egm = egm
        self.name = egm_acctstr(self.egm, c_int(acctno)).decode('utf-8')
        if len(name) > 0:
            self.nerdyname = name + ':' + self.name
        else:
            self.nerdyname = self.name
        self.number = acctno
        self.balance = 0.0
        self.has_child = egm_account_has_child(self.egm, self.number)

    def NextChild(self, acct):
        res = egm_account_get_child(self.egm, self.number, acct)
        return res

    @property
    def Name(self):
        return self.name

    # For the inner grownup in all of us
    @property
    def NerdyName(self):
        return self.nerdyname

    @property
    def HasChild(self):
        return self.has_child

    @property
    def Number(self):
        return self.number

    def Balance(self):
        self.balance = egm_account_value(self.egm, c_int(self.number))
        return float(self.balance)

class EgmEmptyEntry:
    def __init__(self, egm, date):
        self.egm = egm
        self.entry = egm_new_entry(date)
        self.finished = False

    def Append(self):
        # XXX: More Python-y error handling
        if not self.finished:
            return -1
        if not self.Balanced:
            return -1
        return egm_entry_append(self.egm, self.entry)

    def AddTrans(self, acct, amt):
        if egm_entry_add_trans(self.egm, self.entry, acct, amt) < 0:
            return -1
        self.finished = True
        return 0

    #@x._setter
    def Notes(self, msg):
        egm_entry_add_notes(self.entry, msg)

    @property
    def Notes(self):
        return egm_entry_get_notes(self.entry)

    @property
    def Balanced(self):
        if egm_entry_balanced(self.entry):
            res = True
        else:
            res = False
        return res

class EgmEntry:
    def __init__(self, egm, entry):
        self.egm = egm
        self.entry = entry
        if self.entry == None:
            self.entry = self.Next()
            # TODO: if still none...
        self.date = egm_entry_date(self.entry)
        self.stamp = egm_entry_stamp(self.entry)

    def Next(self):
        return egm_next_entry(self.egm, self.entry)

    @property
    def Date(self):
        return self.date

    @property
    def Stamp(self):
        return self.stamp

    # TODO: trans stuff, eothers

class EgmClass:
    def __init__(self):
        self.egm = egm_init()
        self.accts = []
        self._init_acct_db('', 0)

    def Readdb(self, dirs=None):
        res = egm_readdb(self.egm, dirs, c_int(1))
        if res < 0:
            self.Perror("readdb() = {0}".format(res))

    # format it yourself
    def Perror(self, s):
        egm_perror(create_string_buffer(s.encode('utf-8')))

    def Pracct(self):
        for a in self.accts:
            print('{0:03d} {1:50} Balance: {2:18.2f}'.format(
                  a.Number, addquot(a.NerdyName), a.Balance()))

    # Fills the locally-stored array of accounts in hierarchical
    # manner.  eg. If account 100 has children, fills all its
    # chilren in subsequent indices before continuing to its
    # sibling, and so on recursively
    def _init_acct_db(self, name, acctno):
        a = EgmAccount(self.egm, name, acctno)
        self.accts.append(a)
        child = a.NextChild(0)
        while child != 0:
            self._init_acct_db(a.NerdyName, child)
            child = a.NextChild(child)


def main():
    egm = EgmClass()
    egm.Readdb()
    egm.Pracct();

if __name__ == '__main__':
    main()
