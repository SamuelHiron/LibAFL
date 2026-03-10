use std::{
    env,
    process::{Command, exit},
};

fn main() {
    if cfg!(windows) {
        println!("cargo:warning=No support for windows yet.");
        exit(0);
    }

    let cwd = env::current_dir().unwrap().to_string_lossy().to_string();

    // Compile le target sans instrumentation (gcc simple)
    Command::new("gcc")
        .args(["src/program.c", "-o"])
        .arg(format!("{cwd}/target/release/program"))
        .status()
        .unwrap();

    // Compile le proxy forkserver (binaire standalone)
    Command::new("gcc")
        .args(["src/forkserver_proxy.c", "-o"])
        .arg(format!("{cwd}/target/release/forkserver_proxy"))
        .status()
        .unwrap();

    // Copier le proxy dans le répertoire courant (forkserver_simple/)
    Command::new("cp")
        .args([
            format!("{cwd}/target/release/forkserver_proxy"),
            format!("{cwd}/forkserver_proxy"),
        ])
        .status()
        .unwrap();

    // ✅ copier le fuzzer dans le répertoire courant (forkserver_simple/)
    Command::new("cp")
        .args([
            format!("{cwd}/target/release/forkserver_simple"),
            format!("{cwd}/forkserver_simple"),
        ])
        .status()
        .unwrap();
    
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=src/");
}
