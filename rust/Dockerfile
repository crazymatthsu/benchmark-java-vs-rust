FROM rust:1-bookworm AS build
WORKDIR /app
COPY Cargo.toml build.rs ./
COPY src ./src
RUN cargo build --release

FROM debian:bookworm-slim
COPY --from=build /app/target/release/rust-bench /usr/local/bin/rust-bench
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh
ENTRYPOINT ["/docker-entrypoint.sh"]
