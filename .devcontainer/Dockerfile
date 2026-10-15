FROM espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805

ARG USER_UID=1000
ARG USER_GID=1000

RUN python3 -m venv /opt/uv \
    && /opt/uv/bin/python -m pip install --no-cache-dir uv==0.7.13 \
    && ln -s /opt/uv/bin/uv /usr/local/bin/uv \
    && groupadd --gid "${USER_GID}" vscode \
    && useradd --uid "${USER_UID}" --gid "${USER_GID}" --create-home --shell /bin/bash vscode \
    && usermod -aG dialout vscode

ENV HOME=/home/vscode

USER vscode
WORKDIR /workspaces/mqtt-espnow
