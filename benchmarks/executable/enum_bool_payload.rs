// Rust 2024 source variant for the Restaurant enum-bool-payload workload.

#[derive(Clone, Copy)]
enum ServiceState {
    Offline,
    Flags {
        open: bool,
        staffed: bool,
        stocked: bool,
        licensed: bool,
    },
    Charge { amount: i64 },
    Checked { open: bool, amount: i64 },
}

fn flags(open: bool) -> ServiceState {
    ServiceState::Flags {
        licensed: true,
        open,
        staffed: false,
        stocked: true,
    }
}

fn is_open(state: ServiceState) -> bool {
    match state {
        ServiceState::Checked { open: allowed, amount: _ } => allowed,
        ServiceState::Charge { amount: _ } => false,
        ServiceState::Offline => false,
        ServiceState::Flags { open: allowed, .. } => allowed,
    }
}

fn amount(state: ServiceState) -> i64 {
    match state {
        ServiceState::Charge { amount: value } => value,
        ServiceState::Checked { amount: value, open: _ } => value,
        ServiceState::Offline => 0,
        ServiceState::Flags { open: _, staffed: _, stocked: _, licensed: _ } => 0,
    }
}

fn is_licensed(state: ServiceState) -> bool {
    match state {
        ServiceState::Flags { licensed: valid, .. } => valid,
        ServiceState::Offline => false,
        ServiceState::Charge { amount: _ } => false,
        ServiceState::Checked { open: _, amount: _ } => false,
    }
}

fn main() {
    let open = flags(true);
    let closed = flags(false);
    let charge = ServiceState::Charge { amount: 17 };
    let checked = ServiceState::Checked { amount: 31, open: true };
    let open_result = is_open(open);
    let closed_result = is_open(closed);
    let charge_result = is_open(charge);
    let checked_result = is_open(checked);
    let charge_amount = amount(charge);
    let checked_amount = amount(checked);
    let licensed = is_licensed(closed);
    println!(
        "States {}/{}/{}/{}; charges {}/{}; licensed {}",
        open_result,
        closed_result,
        charge_result,
        checked_result,
        charge_amount,
        checked_amount,
        licensed
    );
}
