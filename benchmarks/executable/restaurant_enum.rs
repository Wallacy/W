// Rust 2024 source variant for the Restaurant payloadless-enum switch workload.

#[derive(Clone, Copy)]
enum Course {
    Starter,
    Main,
    Dessert,
}

fn price(course: Course) -> i64 {
    match course {
        Course::Starter => 10,
        Course::Main => 30,
        Course::Dessert => 20,
    }
}

fn main() {
    println!(
        "Courses {}/{}/{}",
        price(Course::Starter),
        price(Course::Main),
        price(Course::Dessert)
    );
}
