// ATTENTION: Keep in sync with C++ function of the same name in filesystem.cpp and `Generator::escapeAttrForFilename`
function replace_invalid_filename_chars(str) {
    return str.replace(new RegExp(':', 'g'), '_');
}
