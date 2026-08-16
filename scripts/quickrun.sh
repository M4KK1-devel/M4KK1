#! /bin/sh

git clone https://github.com/Project-N-E-K-O/N.E.K.O.git ./N.E.K.O && cd ./N.E.K.O && uv sync && ./build_frontend.sh && uv run python launcher.py
