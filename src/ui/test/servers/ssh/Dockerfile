# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The machine orior's tunnels to a database are tested through: OpenSSH's server, letting the user
# orior in by the key the harness puts on it and by nothing else, and forwarding a port as a tunnel
# asks.

FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends openssh-server \
 && rm -rf /var/lib/apt/lists/* \
 && useradd -m -s /bin/sh orior \
 && usermod -p '*' orior \
 && mkdir -p /run/sshd /home/orior/.ssh \
 && chown orior:orior /home/orior/.ssh \
 && chmod 700 /home/orior/.ssh
COPY start.sh /start.sh
EXPOSE 22
CMD ["sh", "/start.sh"]
