FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY generate_report.py .
ENTRYPOINT ["python", "/app/generate_report.py"]
