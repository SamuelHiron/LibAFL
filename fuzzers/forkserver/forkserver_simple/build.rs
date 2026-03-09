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

    // Compile le target avec seulement la couverture (pas de runtime AFL)
    Command::new("clang")
        .args([
            "-fsanitize-coverage=trace-pc-guard",
            "src/program.c",
            "src/coverage_rt.c",
            "-o",
        ])
        .arg(format!("{cwd}/target/release/program"))
        .status()
        .unwrap();

    // ✅ compiler forkserver_preload.so
    Command::new("gcc")
        .args(["-shared", "-fPIC", "src/forkserver_preload.c", "-o"])
        .arg(format!("{cwd}/target/release/forkserver_preload.so"))
        .status()
        .unwrap();

    // ✅ copier forkserver_preload.so dans le répertoire courant (forkserver_simple/)
    Command::new("cp")
        .args([
            format!("{cwd}/target/release/forkserver_preload.so"),
            format!("{cwd}/forkserver_preload.so"),
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
