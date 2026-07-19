FROM debian:12-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2
FROM debian:12-slim
RUN apt-get update && apt-get install -y --no-install-recommends libcurl4 ca-certificates && rm -rf /var/lib/apt/lists/* && mkdir /data
COPY --from=build /src/build/autopoiesis /usr/local/bin/autopoiesis
COPY --from=build /src/build/autopoiesis_tests /usr/local/bin/autopoiesis_tests
COPY --from=build /src/build/autopoiesis_budget_tests /usr/local/bin/autopoiesis_budget_tests
COPY --from=build /src/build/autopoiesis_goal_tests /usr/local/bin/autopoiesis_goal_tests
COPY --from=build /src/build/autopoiesis_reporting_tests /usr/local/bin/autopoiesis_reporting_tests
COPY --from=build /src/build/autopoiesis_timing_tests /usr/local/bin/autopoiesis_timing_tests
COPY --from=build /src/build/autopoiesis_validation_tests /usr/local/bin/autopoiesis_validation_tests
COPY --from=build /src/build/autopoiesis_checkpoint_tests /usr/local/bin/autopoiesis_checkpoint_tests
COPY --from=build /src/build/autopoiesis_campfire_tests /usr/local/bin/autopoiesis_campfire_tests
COPY --from=build /src/build/autopoiesis_camp_stockpile_tests /usr/local/bin/autopoiesis_camp_stockpile_tests
COPY --from=build /src/build/autopoiesis_collective_camp_tests /usr/local/bin/autopoiesis_collective_camp_tests
ENTRYPOINT ["/usr/local/bin/autopoiesis"]
