FROM gcc:latest

WORKDIR /app

RUN apt-get update && apt-get install -y libreadline-dev

COPY . .

RUN make clean && make

CMD ["./minishell_plus"]