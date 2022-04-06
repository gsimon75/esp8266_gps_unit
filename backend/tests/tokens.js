/*
 * These are one-hour tokens, so don't bother checking them in to git.
 * And choose admin-only, trainer-only and client-only tokens, as we test for the rightfully denied accesses as well.
 */
const tokens = {
    admin: null,
    client: null,
    expired: null,
};

function bearerify(x) {
    return x ? "Bearer " + x : null;
}

exports.admin_id = 2;
exports.admin = bearerify(tokens.admin);
exports.client_id = 3;
exports.client = bearerify(tokens.client);
exports.expired_id = 3;
exports.expired = bearerify(tokens.expired);

// vim: set ts=4 sw=4 et:
