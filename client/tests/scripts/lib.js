function TEST_CONTAINS(data, val) {
    if (data.indexOf(val) < 0) {
        throw new Error(`value '${val}' was not found in the specified data`);
    }
}

function TEST_EQ(one, two) {
    if (one != two) {
        throw new Error(`${one} (type: ${typeof one}) does not equal ${two} (type: ${typeof two})`);
    }
}

function TEST_STRICTEQ(one, two) {
    if (one !== two) {
        throw new Error(`${one} (type: ${typeof one}) does not strictly equal ${two} (type: ${typeof two})`);
    }
}

function TEST_NE(one, two) {
    if (one == two) {
        throw new Error(`${one} (type: ${typeof one}) equals ${two} (type: ${typeof two})`);
    }
}

function TEST_STRICTNE(one, two) {
    if (one === two) {
        throw new Error(`${one} (type: ${typeof one}) strictly equals ${two} (type: ${typeof two})`);
    }
}

function TEST_TRUE(val) {
    if (!val) {
        throw new Error(`${val} is not true`);
    }
}

function TEST_STRICTTRUE(val) {
    if (val !== true) {
        throw new Error(`${val} is not strictly true`);
    }
}

function TEST_FALSE(val) {
    if (val) {
        throw new Error(`${val} is not false`);
    }
}

function TEST_STRICTFALSE(val) {
    if (val !== false) {
        throw new Error(`${val} is not strictly false`);
    }
}

function TEST_THROWS(lambda) {
    try {
        lambda();
        throw new Error(`no error thrown while executing function: ${lambda}`);
    } catch { }
}

function TEST_ARRCONTAINS(arr, key) {
    if (!arr.includes(key)) {
        throw new Error(`${arr} doesn't contain expected key: ${key}`);
    }
}

function TEST_ARRNOTCONTAINS(arr, key) {
    if (arr.includes(key)) {
        throw new Error(`${arr} contain unexpected key: ${key}`);
    }
}

function TEST_ARR_STRICTEQ(arr1, arr2) {
    if (arr1.length !== arr2.length) {
        throw new Error(`${arr1} (length: ${arr1.length}) is not the same length as ${arr2} (length: ${arr2.length})`);
    }

    for (let i = 0; i < arr1.length; i++) {
        TEST_STRICTEQ(arr1[i], arr2[i]);
    }
}

///////////////////////////////////////////////////////////////////////////////

// stub os-specific test funcs
// redefine them in your test case

function TEST_LINUX() { }
function TEST_WINDOWS() { }