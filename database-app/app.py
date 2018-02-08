# -*- encoding: utf-8 -*-
from __future__ import division

import logging

import flask

import superperm

# The flask.Flask constructor has a mandatory string argument
# that has no apparent effect.
app = flask.Flask("I don’t think this string is actually used")

# Exceptions are apparently not logged by default, so we need to
# add an error handler that logs them.
@app.errorhandler(500)
def server_error(e):
    logging.exception("Exception handling request")
    return "500 Server error", 500


@app.route("/")
def route_input():
    return flask.render_template("input.html")

@app.route("/lookup")
def route_lookup():
    q = flask.request.args.get("q")

    try:
        superpermutation = superperm.lookup(q)
        return flask.render_template("output.html", **superpermutation)
    except superperm.Exception, e:
        return flask.render_template("input.html", q=q, error=e.message)
