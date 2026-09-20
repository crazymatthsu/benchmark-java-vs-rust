FROM python:3.12-slim
WORKDIR /app
COPY src ./src
ENV PYTHONPATH=/app/src
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh
ENTRYPOINT ["/docker-entrypoint.sh"]
