FROM debian:12-slim AS cpp-build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      build-essential ca-certificates cmake libcurl4-openssl-dev nlohmann-json3-dev python3 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j2 \
    && ctest --test-dir build --output-on-failure

FROM oven/bun:1.3.14-slim AS web-build

WORKDIR /web
COPY web/package.json web/bun.lock ./
RUN bun install --frozen-lockfile
COPY web/ ./
RUN bun run typecheck \
    && bun test \
    && bun run build

FROM oven/bun:1.3.14-slim AS runtime

USER root
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates libcurl4 \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /app/build /app/web /data \
    && chown -R bun:bun /app /data

WORKDIR /app
COPY --from=cpp-build --chown=bun:bun /src/build/autopoiesis /app/build/autopoiesis
COPY --from=cpp-build --chown=bun:bun /src/build/autopoiesis_backend /app/build/autopoiesis_backend
COPY --from=web-build --chown=bun:bun /web/package.json /web/bun.lock /app/web/
COPY --from=web-build --chown=bun:bun /web/node_modules /app/web/node_modules
COPY --from=web-build --chown=bun:bun /web/dist /app/web/dist
COPY --from=web-build --chown=bun:bun /web/server /app/web/server
COPY --from=web-build --chown=bun:bun /web/src /app/web/src

ENV PORT=3000 \
    AUTOPOIESIS_BACKEND_PATH=/app/build/autopoiesis_backend \
    AUTOPOIESIS_DATA_DIR=/data
USER bun
EXPOSE 3000
CMD ["bun", "run", "--cwd", "/app/web", "start"]
